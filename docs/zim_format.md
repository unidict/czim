# ZIM File Format Specification

> This document describes the ZIM archive file format, based on the
> [openZIM specification](https://wiki.openzim.org/wiki/ZIM_file_format)
> and the czim C library implementation.

ZIM is a container file format for storing offline web content (such as
Wikipedia). A ZIM file is a single binary file that stores all entries
(articles, images, metadata) in a structured, compressed, and indexed
format optimized for efficient random access.

## Overview

A ZIM archive has the following logical layout:

```
+-------------------+
| Header (80 bytes) |
+-------------------+
| MIME Type List    |
+-------------------+
| Path Ptr List     |
+-------------------+
| Title Ptr List    |
+-------------------+
| Cluster Ptr List  |
+-------------------+
| Directory Entries |
+-------------------+
| Clusters          |
+-------------------+
| Checksum (MD5)    |
+-------------------+
```

All multi-byte integers in the ZIM format are stored in **little-endian**
byte order and are **unsigned** (`uint16`, `uint32`, `uint64`).

---

## Header

The ZIM archive begins with an 80-byte header. The header contains
offsets and counts that describe the rest of the file.

| Offset | Size | Field            | Description                                            |
|--------|------|------------------|--------------------------------------------------------|
| 0      | 4    | magicNumber      | Magic number: `0x044D495A` (ASCII `ZIM\x04`)          |
| 4      | 2    | majorVersion     | Major version (5 or 6)                                 |
| 6      | 2    | minorVersion     | Minor version (>= 1 means new namespace scheme)        |
| 8      | 16   | uuid             | Unique identifier (16 raw bytes)                       |
| 24     | 4    | entryCount       | Total number of directory entries                       |
| 28     | 4    | clusterCount     | Total number of clusters                                |
| 32     | 8    | pathPtrPos       | File offset of the path pointer list                    |
| 40     | 8    | titlePtrPos      | File offset of the title pointer list (0 = absent)     |
| 48     | 8    | clusterPtrPos    | File offset of the cluster pointer list                 |
| 56     | 8    | mimeListPos      | File offset of the MIME type list (= header size)       |
| 64     | 4    | mainPage         | Entry index of the main page (`0xFFFFFFFF` if none)    |
| 68     | 4    | layoutPage       | Layout page index (deprecated, always `0xFFFFFFFF`)    |
| 72     | 8    | checksumPos      | File offset of the MD5 checksum (16 bytes before EOF)  |

**Total header size: 80 bytes.**

### Version History

The major version determines fundamental format compatibility:

| Major | Minor | Description                                    |
|-------|-------|------------------------------------------------|
| 5     | -     | Path index introduced; single version number   |
| 5     | 0     | Major/Minor version distinction introduced     |
| 6     | 0     | Extended clusters (64-bit offsets); old namespace scheme |
| 6     | 1     | New namespace scheme                            |
| 6     | 2     | Alias entries allowed (multiple entries per blob) |
| 6     | 3     | Title index listing removed from search indexes |

---

## MIME Type List

Located at `mimeListPos` (which also marks the end of the header). The
MIME type list is a sequence of zero-terminated UTF-8 strings, ending
with an empty string (a single `\0` byte).

```
"text/html\0"
"image/png\0"
"application/javascript\0"
"\0"                        <-- end of list
```

Directory entries reference MIME types by their **0-based index** in this
list. For example, if `"text/html"` is the first entry, a content entry
with `mimetype = 0` has MIME type `text/html`.

---

## Path Pointer List

Located at `pathPtrPos`. An array of `entryCount` entries, each being a
**64-bit file offset** pointing to a directory entry.

| Offset in list | Size | Description                          |
|----------------|------|--------------------------------------|
| 0              | 8    | File offset of entry #0              |
| 8              | 8    | File offset of entry #1              |
| (n-1) * 8      | 8    | File offset of entry #(n-1)          |

Directory entries are **sorted by full path** (`namespace + path`,
UTF-8 byte comparison). This sorted order enables binary search for
path-based lookups.

The index of an entry in this list is the **path index** (also called
entry index), used throughout the format to identify entries.

---

## Title Pointer List

Located at `titlePtrPos` (may be `0` if absent). An array of
`entryCount` entries, each being a **32-bit path index** (not a file
offset).

| Offset in list | Size | Description                                |
|----------------|------|--------------------------------------------|
| 0              | 4    | Path index of entry sorted 1st by title    |
| 4              | 4    | Path index of entry sorted 2nd by title    |
| (n-1) * 4      | 4    | Path index of entry sorted nth by title    |

**Indirection**: To resolve a title index to a file offset:

1. Read the 32-bit path index from the title pointer list
2. Use that path index to look up the 64-bit file offset in the path
   pointer list
3. Read the directory entry at that file offset

This two-level indirection halves the title list size (4 bytes per entry
vs 8 bytes) and allows reusing cached directory entries.

---

## Directory Entries

Directory entries (also called "dirents") store metadata for every item
in the archive. There are two main types, distinguished by the
`mimetype` field:

### Content Entry

For actual content (articles, images, etc.). The `mimetype` field
contains a valid MIME type index (not `0xFFFF`).

| Offset | Size | Field            | Description                                    |
|--------|------|------------------|------------------------------------------------|
| 0      | 2    | mimetype         | MIME type index (index into MIME type list)     |
| 2      | 1    | parameterLen     | Extra parameter length (always 0)               |
| 3      | 1    | namespace        | Namespace character (e.g. `'C'`, `'M'`, `'W'`) |
| 4      | 4    | revision         | Revision number (always 0)                      |
| 8      | 4    | clusterNumber    | Cluster index containing the data               |
| 12     | 4    | blobNumber       | Blob index within the cluster                   |
| 16     | var  | path             | Zero-terminated UTF-8 path string               |
| var    | var  | title            | Zero-terminated UTF-8 title (empty = use path)  |
| var    | var  | parameter        | Extra parameters (length = parameterLen)        |

**Variable-size entry**: The total size depends on the path and title
string lengths.

### Redirect Entry

For entries that point to another entry. Identified by `mimetype ==
0xFFFF`.

| Offset | Size | Field            | Description                                    |
|--------|------|------------------|------------------------------------------------|
| 0      | 2    | mimetype         | `0xFFFF` (redirect sentinel)                    |
| 2      | 1    | parameterLen     | Extra parameter length (always 0)               |
| 3      | 1    | namespace        | Namespace character                             |
| 4      | 4    | revision         | Revision number (always 0)                      |
| 8      | 4    | redirectIndex    | Path index of the redirect target               |
| 12     | var  | path             | Zero-terminated UTF-8 path string               |
| var    | var  | title            | Zero-terminated UTF-8 title (empty = use path)  |
| var    | var  | parameter        | Extra parameters (length = parameterLen)        |

### Deprecated Entry Types

These may appear in very old ZIM files and should be ignored by readers:

- `mimetype == 0xFFFE` -- Linktarget entry (deprecated)
- `mimetype == 0xFFFD` -- Deleted entry (deprecated)

---

## Cluster Pointer List

Located at `clusterPtrPos`. An array of `clusterCount` entries, each
being a **64-bit file offset** pointing to a cluster.

| Offset in list | Size | Description                          |
|----------------|------|--------------------------------------|
| 0              | 8    | File offset of cluster #0            |
| 8              | 8    | File offset of cluster #1            |
| (n-1) * 8      | 8    | File offset of cluster #(n-1)        |

---

## Clusters

Clusters contain the actual data. Multiple directory entries can share
one cluster, allowing their data to be compressed together for better
compression ratios. Typical cluster size is around **1 MB** (compressed).

### Cluster Info Byte

The first byte of a cluster encodes compression type and extended flag:

```
Bit layout:  [E . . . C C C C]
              ^       ^^^^^^^
              |       Compression type (bits 0-3)
              Extended flag (bit 4)
```

| Bits 0-3 | Compression            |
|----------|------------------------|
| 0        | No compression (obsolete, inherited from Zeno) |
| 1        | No compression         |
| 2        | zlib (deprecated)      |
| 3        | bzip2 (deprecated)     |
| 4        | LZMA                   |
| 5        | Zstandard (zstd)       |

| Bit 4 | Offset size                              |
|-------|------------------------------------------|
| 0     | 4-byte (32-bit) offsets, max 4 GB data  |
| 1     | 8-byte (64-bit) offsets (extended cluster) |

Extended clusters are only valid in major version 6 archives.

### Blob Offset Table

Following the info byte, the decompressed data begins with a blob offset
table. There are always **N+1 offsets** for **N blobs**. The last offset
points to the end of the data area.

```
Byte 0:              Info byte (compression + extended flag)
Byte 1..OFFSET_SIZE: Offset to blob #0
                     Offset to blob #1
                     ...
                     Offset to blob #(N-1)
                     Offset to end of data (sentinel)
--- data area ---
                     Blob #0 data
                     Blob #1 data
                     ...
                     Blob #(N-1) data
```

**Calculating blob count**:

```
blob_count = (first_offset / OFFSET_SIZE) - 1
```

Where `OFFSET_SIZE` is 4 for normal clusters or 8 for extended clusters.

**Calculating blob size**:

```
blob[i].size = offset[i+1] - offset[i]
```

All offset values are **little-endian unsigned integers** and address
positions within the decompressed data (starting from byte 1, after the
info byte).

### Data Access Flow

To read blob data from a compressed cluster:

1. Read the cluster info byte
2. Create a streaming decompression context
3. Decompress the offset table to determine blob boundaries
4. On demand, continue streaming decompression to read individual blob data

For uncompressed clusters (`compression == 1`), data can be read
directly without decompression.

---

## Namespaces

Namespaces separate different types of directory entries that might
otherwise have the same path or title. The namespace is the first
character of the full path stored in the directory entry.

### New Namespace Scheme (minorVersion >= 1)

| Namespace | Description                            |
|-----------|----------------------------------------|
| `C`       | User content (articles, HTML, images)  |
| `M`       | ZIM metadata (Counter, Title, etc.)    |
| `W`       | Well-known entries (mainPage, favicon) |
| `X`       | Search indexes                         |

### Old Namespace Scheme (minorVersion < 1)

| Namespace | Description               |
|-----------|---------------------------|
| `-`       | Layout (CSS, JS, images)  |
| `A`       | Articles                  |
| `B`       | Article metadata          |
| `I`       | Images / files            |
| `J`       | Images / text             |
| `M`       | Metadata                  |
| `U`       | Categories / text         |
| `V`       | Categories / article list |
| `W`       | Categories per article    |
| `X`       | Search indexes            |

The library hides namespaces from the user. For example, entry
`C/index.html` is accessible simply as `index.html`.

---

## Paths

- Paths in the ZIM archive are **UTF-8 encoded** and are **not**
  URL-encoded.
- Paths are sorted by full path (`namespace + path`) using UTF-8 byte
  comparison.
- Directory entries store the path **without** a leading `/`.
- If the title field is empty, the path is used as the title.

---

## Checksum

Located at `checksumPos`. A 16-byte **MD5** hash of the entire archive
(excluding the checksum itself). The checksum position is always
`filesize - 16`.

---

## Split ZIM Archives

ZIM archives larger than 4 GB can be split into chunks for
filesystems with file size limits (e.g. FAT32). Splitting must occur on
cluster boundaries. Chunk files use alphabetical extensions:

```
wikipedia.zimaa
wikipedia.zimab
wikipedia.zimac
...
```

---

## Integer Encoding

- All integers are **unsigned** and **little-endian**.
- Common sizes: `uint8`, `uint16`, `uint32`, `uint64`.
- All sizes are in bytes.

---

## Complete Access Flow

The following diagram shows how to look up an entry by path:

```
1. Read header (80 bytes)
2. Read MIME type list from mimeListPos
3. Binary search in path pointer list (at pathPtrPos)
   - Read path_ptrs[i] (8 bytes) to get directory entry file offset
   - Read directory entry at that offset
   - Compare entry path with target path
4. Found entry is either:
   a) Content entry: read cluster from cluster_ptrs[clusterNumber]
      - Decompress cluster
      - Extract blob[blobNumber]
   b) Redirect entry: follow redirectIndex to another path index,
      go to step 3
```

To look up an entry by title:

```
1. Binary search in title pointer list (at titlePtrPos)
   - Read title_ptrs[i] (4 bytes) to get path index
   - Read path_ptrs[path_index] (8 bytes) to get file offset
   - Read directory entry at that offset
   - Compare entry title with target title
2. Same content/redirect resolution as path lookup
```
