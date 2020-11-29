#include <docview.hpp>

#include <cstdio>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <fstream>

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
    std::vector<std::pair<const docview::doc_tree_node*, std::filesystem::path>> root_nodes;
    std::vector<std::filesystem::path> temp_files;

    void delete_node(const docview::doc_tree_node* node)
    {
        for (auto& child : node->children)
            delete_node(child);

        delete node;
    }

public:
    manview()
        : root_nodes()
    {}

    ~manview()
    {
        for (auto& root : root_nodes)
            delete_node(root.first);
        for (auto& temp : temp_files)
            execute("rm -f " + std::string(temp));
    }

    applicability_level get_applicability_level() noexcept
    {
        return applicability_level::medium;
    }

    const docview::doc_tree_node* get_doc_tree(std::filesystem::path path) noexcept
    {
        for (auto& root : root_nodes)
            if (root.second == path)
                return root.first;

        docview::doc_tree_node* root_node = nullptr;
        std::string man_output = execute(
            "MANPATH='" + std::string(path) + "' man -k . 2>&1 | awk '{print $1\" \"$2}'"
        );

        if (man_output == ".: nothing\n")
        {
            return nullptr;
        }

        root_node = new docview::doc_tree_node;
        root_node->parent = nullptr;
        root_node->title = "Man pages: " + std::string(path);
        root_node->synonyms = {"man", std::string(path)};

        std::map<std::string, std::vector<std::string>> sections;

        // Parse the output
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

        for (auto& section : sections)
        {
            docview::doc_tree_node* section_node = new docview::doc_tree_node;
            section_node->parent = root_node;
            section_node->title = "Section " + section.first;
            section_node->synonyms = {section.first};
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

    std::pair<std::string, bool> get_doc(const docview::doc_tree_node* node) noexcept
    {
        if (!node->parent || !node->parent->parent)
            return std::make_pair("<html></html>", false);

        const docview::doc_tree_node* root_node = node->parent->parent;
        std::filesystem::path path;
        for (auto& root : root_nodes)
            if (root.first == root_node)
                path = root.second;

        std::filesystem::path temp = execute("echo -n `mktemp`");
        temp_files.push_back(temp);
        execute(
            "MAN_KEEP_FORMATTING=1 COLUMNS=80 MANPATH='" + std::string(path) +
            "' man -P cat '" + node->title + "(" + node->parent->title.substr(8) +
            ")' | ul | aha --black --title '" + node->title + "(" +
            node->parent->title.substr(8) + ")" +
            "' > " + std::string(temp)
        );
        return std::make_pair("file://" + std::string(temp), true);
    }

    std::string brief(const docview::doc_tree_node* node) noexcept
    {
        if (!node->parent || !node->parent->parent)
            return std::string();

        const docview::doc_tree_node* root_node = node->parent->parent;
        std::filesystem::path path;
        for (auto& root : root_nodes)
            if (root.first == root_node)
                path = root.second;

        return execute(
            "MANPATH='" + std::string(path) + "' man -f '" + node->title +
            "' | grep -- '" + node->title + "(" + node->parent->title.substr(8) +
            ")'"
        );
    }

    std::string details(const docview::doc_tree_node* node) noexcept
    {
        return section(node, "DESCRIPTION");
    }

    std::string section(const docview::doc_tree_node* node, std::string name) noexcept
    {
        if (!node->parent || !node->parent->parent)
            return std::string();

        const docview::doc_tree_node* root_node = node->parent->parent;
        std::filesystem::path path;
        for (auto& root : root_nodes)
            if (root.first == root_node)
                path = root.second;

        std::string man_output = execute(
            "COLUMNS=80 MANPATH='" + std::string(path) + "' man -P cat '" +
            node->title + "(" + node->parent->title.substr(8) + ")'"
        );

        std::string section;

        std::stringstream lines(man_output);
        bool section_started = false;
        for (std::string line; std::getline(lines, line);)
        {
            if (line.substr(0, name.size()) == name)
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

        return section;
    }
};

manview extension_object;
