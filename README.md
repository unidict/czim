# czim

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C](https://img.shields.io/badge/C-11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard))
[![CI](https://github.com/unidict/czim/actions/workflows/ci.yml/badge.svg)](https://github.com/unidict/czim/actions/workflows/ci.yml)

A fast, lightweight C library for reading [ZIM archive](https://wiki.openzim.org/wiki/ZIM_file_format) files. ZIM is the format used by [Kiwix](https://www.kiwix.org/) to store offline web content such as Wikipedia, Stack Exchange, and more.

## Overview

czim provides a C11 API for reading ZIM archives with support for:

- **Streaming decompression** — Incremental Zstandard and LZMA decompression for efficient memory usage
- **Path and title lookup** — Binary search with NarrowDown sparse index for fast O(log n) entry retrieval
- **Prefix search** — Range-based prefix matching for both paths and titles
- **Redirect resolution** — Automatic redirect chain following
- **Blob access** — On-demand blob decompression with LRU cluster caching
- **Metadata and illustrations** — Access to archive metadata, favicon, and illustration images

## Prerequisites

- C11 compiler (Clang, GCC, MSVC)
- [zstd](https://github.com/facebook/zstd) >= 1.4
- [liblzma](https://tukaani.org/xz/) >= 5.2
- [CMake](https://cmake.org/) >= 3.14
- [uobject](https://github.com/unidict/uobject) (included as git submodule)

## Build

```bash
# Clone with submodules
git clone --recursive https://github.com/unidict/czim.git
cd czim

# Configure
cmake -B build -DCZIM_BUILD_TESTS=ON

# Build
cmake --build build --config Release

# Test
ctest --test-dir build --verbose

# Install
cmake --install build --prefix /usr/local
```

### CMake Options

| Option                  | Default | Description           |
|-------------------------|---------|-----------------------|
| `CZIM_BUILD_TESTS`      | ON      | Build test suite      |
| `BUILD_SHARED_LIBS`     | OFF     | Build shared library  |

## Quick Start

```c
#include "czim_archive.h"
#include "czim_archive_ext.h"
#include <stdio.h>

int main(void) {
    // Open archive
    czim_archive *archive = czim_archive_open("wikipedia.zim");
    if (!archive) return 1;

    // Query properties
    printf("Entries: %u\n", czim_archive_entry_count(archive));
    printf("Articles: %u\n", czim_archive_article_count(archive));

    // Find entry by path
    uint32_t index;
    czim_entry *entry = czim_archive_find_entry_by_path(archive, 'C', "African_Americans", &index);
    if (entry) {
        printf("Found: %s\n", czim_entry_get_path(entry));

        // Read blob data
        if (!czim_entry_is_redirect(entry)) {
            czim_blob blob = czim_archive_get_blob(archive, entry);
            printf("Size: %zu bytes\n", blob.size);
            czim_blob_free(&blob);
        }
        czim_entry_free(entry);
    }

    czim_archive_close(archive);
    return 0;
}
```

## API Overview

### Archive Lifecycle

| Function                | Description                   |
|-------------------------|-------------------------------|
| `czim_archive_open()`   | Open a ZIM archive            |
| `czim_archive_close()`  | Close an archive              |

### Archive Properties

| Function                          | Description                             |
|-----------------------------------|-----------------------------------------|
| `czim_archive_uuid()`             | Get archive UUID                        |
| `czim_archive_entry_count()`      | Total number of entries                 |
| `czim_archive_article_count()`    | Number of front articles                |
| `czim_archive_cluster_count()`    | Number of clusters                      |
| `czim_archive_has_main_entry()`   | Check if main page is set               |
| `czim_archive_has_title_index()`  | Check if title index exists             |

### Entry Access

| Function                            | Description                          |
|-------------------------------------|--------------------------------------|
| `czim_archive_get_entry_by_index()` | Get entry by path index              |
| `czim_archive_find_entry_by_path()` | Find entry by path + namespace       |
| `czim_archive_find_entry_by_title()`| Find entry by title + namespace      |
| `czim_archive_resolve_redirect()`   | Follow redirect chain                |
| `czim_archive_find_metadata()`      | Find metadata entry by name          |

### Prefix Search

| Function                                    | Description                      |
|---------------------------------------------|----------------------------------|
| `czim_archive_find_entry_by_path_prefix()`  | Find entry range by path prefix  |
| `czim_archive_find_entry_by_title_prefix()` | Find entry range by title prefix |

### Blob Access

| Function                    | Description                          |
|-----------------------------|--------------------------------------|
| `czim_archive_get_blob()`   | Get blob data for a content entry    |

See `czim_archive.h` and `czim_archive_ext.h` for the complete API documentation.

## Platform Support

| Platform | Status       |
|----------|--------------|
| macOS    | Tested       |
| Linux    | Tested (CI)  |
| Windows  | Tested (CI)  |

## Documentation

- [ZIM File Format](docs/zim_format.md) — Detailed ZIM format specification

## License

MIT License. See [LICENSE](LICENSE) for details.

## Acknowledgments

- [openZIM](https://openzim.org/) — ZIM file format specification
- [libzim](https://github.com/openzim/libzim) — Reference C++ implementation
- [zstd](https://github.com/facebook/zstd) — Zstandard compression by Facebook
- [liblzma](https://tukaani.org/xz/) — LZMA/XZ compression
- [ThrowTheSwitch/Unity](https://github.com/ThrowTheSwitch/Unity) — C test framework
