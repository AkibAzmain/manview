# Manview

An extension for viewing man pages with Docview.

## Usage

First enable it in Docview. Then add directory with man pages to documentation search paths (e.g `/usr/share/man`). If the directory contains any valid man pages, it'll appear in the sidebar as "Man pages: [directory]".

## Compiling

Just execute `make` . If you've installed Docview in custom directory, set proper compiler arguments in `CXXFLAGS` and `LDFLAGS` variables.

### Dependencies

This extension depends on libdocview, aha and the C++ standard library.
