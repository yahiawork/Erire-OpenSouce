#ifndef FIRSTSTAND_FORMAT_H
#define FIRSTSTAND_FORMAT_H

#include <stdint.h>

#define FS_MAGIC "FIRSTSTANDPKG1"
#define FS_MAGIC_SIZE 16
#define FS_ARCHIVE_VERSION 1

#pragma pack(push, 1)
typedef struct FsFooter {
    char magic[FS_MAGIC_SIZE];
    uint64_t archive_offset;
    uint64_t archive_size;
    uint32_t version;
    uint32_t flags;
} FsFooter;
#pragma pack(pop)

#endif
