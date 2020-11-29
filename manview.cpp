#include <docview.hpp>

#include <cstdio>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <fstream>

// Executes a command and returns it's output
std::string execute(std::string command) {
    std::string output;
    std::array<char, 128> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }
    return output;
}

class manview : public docview::extension
{
private:

    // List of all root nodes created
    std::vector<std::pair<const docview::doc_tree_node*, std::filesystem::path>> root_nodes;

    // List of all temporary files created
    std::vector<std::filesystem::path> temp_files;

    // Deletes a node with all of it's children
    void delete_node(const docview::doc_tree_node* node)
    {

        // Delete all children
        for (auto& child : node->children)
            delete_node(child);

        // Delete the node
        delete node;
    }

public:

    // Contructor
    manview()
        : root_nodes(),
        temp_files()
    {}

    // Destructor, releases memory and disk space
    ~manview()
    {

        // Delete all nodes
        for (auto& root : root_nodes)
            delete_node(root.first);
    
        // Delete all temporary files
        for (auto& temp : temp_files)
            execute("rm -f " + std::string(temp));
    }

    // Applicability level, whose value is medium
    applicability_level get_applicability_level() noexcept
    {
        return applicability_level::medium;
    }

    // Generates a document tree from an directory containing man pages
    const docview::doc_tree_node* get_doc_tree(std::filesystem::path path) noexcept
    {

        // If this directory is successfully parsed already, return previous nodes
        for (auto& root : root_nodes)
            if (root.second == path)
                return root.first;

        // The root node
        docview::doc_tree_node* root_node = nullptr;
        std::string man_output = execute(
            "MANPATH='" + std::string(path) + "' man -k . 2>&1 | awk '{print $1\" \"$2}'"
        );

        // If man says ".: nothing appropiate.\n" (because of awk, it would be just ".: nothing\n"),
        // it's invalid path, therefore return nullptr
        if (man_output == ".: nothing\n")
        {
            return nullptr;
        }

        // Create and setup root node
        root_node = new docview::doc_tree_node;
        root_node->parent = nullptr;
        root_node->title = "Man pages: " + std::string(path);
        root_node->synonyms = {"man", std::string(path)};

        // Map of sections with all pages
        std::map<std::string, std::vector<std::string>> sections;

        // Parse man output
        {
            std::stringstream stream(man_output);
            for (std::string line; std::getline(stream, line);)
            {
                std::string name, section;
                std::stringstream linestream(line);
                linestream >> name >> section;
                section.erase(section.begin());
                section.pop_back();
                sections[section].push_back(name);
            }
        }

        // Generate tree for every section with all documents in it
        for (auto& section : sections)
        {

            // Create and setup section node
            docview::doc_tree_node* section_node = new docview::doc_tree_node;
            section_node->parent = root_node;
            section_node->title = "Section " + section.first;
            section_node->synonyms = {section.first};

            // Create nodes for all documents in the section
            for (auto& page : section.second)
            {
                docview::doc_tree_node* page_node = new docview::doc_tree_node;
                page_node->parent = section_node;
                page_node->title = page;
                section_node->children.push_back(page_node);
            }
            root_node->children.push_back(section_node);
        }

        root_nodes.push_back(std::make_pair(root_node, path));
        return root_node;
    }

    // Returns the actual document of a node
    std::pair<std::string, bool> get_doc(const docview::doc_tree_node* node) noexcept
    {

        // If node is root node or section node, empty page
        if (!node->parent || !node->parent->parent)
            return std::make_pair("<html></html>", false);

        // Get the directory of man pages
        const docview::doc_tree_node* root_node = node->parent->parent;
        std::filesystem::path path;
        for (auto& root : root_nodes)
            if (root.first == root_node)
                path = root.second;

        // Create a temporary empty file
        std::filesystem::path temp = execute("echo -n `mktemp`");
        temp_files.push_back(temp);

        // Convert man output to html and save to temporary file
        execute(
            "MAN_KEEP_FORMATTING=1 COLUMNS=80 MANPATH='" + std::string(path) +
            "' man -P cat '" + node->title + "(" + node->parent->title.substr(8) +
            ")' | ul | aha --black --title '" + node->title + "(" +
            node->parent->title.substr(8) + ")" +
            "' > " + std::string(temp)
        );

        // Return URI to temporary file
        return std::make_pair("file://" + std::string(temp), true);
    }

    // Returns the brief of a man pages
    std::string brief(const docview::doc_tree_node* node) noexcept
    {

        // If node is root node or section node, empty page
        if (!node->parent || !node->parent->parent)
            return std::string();

        // Get the directory of man pages
        const docview::doc_tree_node* root_node = node->parent->parent;
        std::filesystem::path path;
        for (auto& root : root_nodes)
            if (root.first == root_node)
                path = root.second;

        // Return the output of "man -f" aka whatis
        return execute(
            "MANPATH='" + std::string(path) + "' man -f '" + node->title +
            "' | grep -- '" + node->title + " (" + node->parent->title.substr(8) +
            ")'"
        );
    }

    // Returns details of a document node
    std::string details(const docview::doc_tree_node* node) noexcept
    {

        // Return the section DESCRIPTION
        return section(node, "DESCRIPTION");
    }

    // Returns a section of a document node
    std::string section(const docview::doc_tree_node* node, std::string name) noexcept
    {

        // If node is root node or section node, empty page
        if (!node->parent || !node->parent->parent)
            return std::string();

        // Get the directory of man pages
        const docview::doc_tree_node* root_node = node->parent->parent;
        std::filesystem::path path;
        for (auto& root : root_nodes)
            if (root.first == root_node)
                path = root.second;

        // Execute man to get the page
        std::string man_output = execute(
            "COLUMNS=80 MANPATH='" + std::string(path) + "' man -P cat '" +
            node->title + "(" + node->parent->title.substr(8) + ")'"
        );

        // Variable to hold section contents
        std::string section;

        /*
            Parse man output to get the section. This isn't fully accurate.
            A man is usually formatted like this:

            section start-> ...
                            SYNOPSIS
                                ...
            section end  ->
            section start-> DESCRIPTION
                                ...
            section end  ->
                            ...
                                ...

            The following code takes advantage of this pattern and uses the
            pattern to identify sections.
        */
        std::stringstream lines(man_output);
        bool section_started = false;
        for (std::string line; std::getline(lines, line);)
        {
            if (line == name)
            {
                section_started = true;
                section += line + '\n';
            }

            if (section_started)
            {
                if ((line[0] == ' ' || line[0] == '\t' || line == ""))
                {
                    section += line + '\n';
                }
                else
                {
                    break;
                }
            }
        }

        // Return section contents, maybe empty
        return section;
    }
};

extern "C"
{

    // The extension object
    manview extension_object;
}
