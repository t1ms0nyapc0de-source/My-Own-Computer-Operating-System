#include "tar.h"
#include "vfs.h"
#include "terminal.h"
#include <string.h>

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
} __attribute__((packed));

#define MAX_TAR_FILES 32
static fs_node_t tar_nodes[MAX_TAR_FILES];
static int tar_node_count = 0;

static fs_node_t tar_root;

static uint32_t parse_octal(const char* str, int max_len) {
    uint32_t val = 0;
    for (int i = 0; i < max_len; i++) {
        if (str[i] == '\0' || str[i] == ' ') break;
        if (str[i] >= '0' && str[i] <= '7') {
            val = val * 8 + (str[i] - '0');
        }
    }
    return val;
}

static uint32_t tar_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint8_t* data = (uint8_t*)node->impl;
    if (offset >= node->length) return 0;
    if (offset + size > node->length) {
        size = node->length - offset;
    }
    memcpy(buffer, data + offset, size);
    return size;
}

static fs_node_t* tar_readdir(fs_node_t* node, uint32_t index) {
    if (node == &tar_root && index < (uint32_t)tar_node_count) {
        return &tar_nodes[index];
    }
    return 0;
}

static fs_node_t* tar_finddir(fs_node_t* node, const char* name) {
    if (node == &tar_root) {
        for (int i = 0; i < tar_node_count; i++) {
            if (strcmp(tar_nodes[i].name, name) == 0) {
                return &tar_nodes[i];
            }
        }
    }
    return 0;
}

void tar_init(uint32_t tar_start, uint32_t tar_end) {
    // Initialize root directory node
    strcpy(tar_root.name, "/");
    tar_root.flags = FS_DIRECTORY;
    tar_root.length = 0;
    tar_root.impl = 0;
    tar_root.read = 0;
    tar_root.write = 0;
    tar_root.open = 0;
    tar_root.close = 0;
    tar_root.readdir = tar_readdir;
    tar_root.finddir = tar_finddir;

    fs_root = &tar_root;

    uint32_t addr = tar_start;
    while (addr < tar_end) {
        struct tar_header* header = (struct tar_header*)addr;
        if (header->name[0] == '\0') {
            break;
        }

        uint32_t size = parse_octal(header->size, 12);
        
        char* filename = header->name;
        if (filename[0] == '.' && filename[1] == '/') {
            filename += 2;
        }

        // Check if we have a valid file and slot available
        if (filename[0] != '\0' && header->typeflag != '5' && tar_node_count < MAX_TAR_FILES) {
            fs_node_t* node = &tar_nodes[tar_node_count++];
            strcpy(node->name, filename);
            node->flags = FS_FILE;
            node->length = size;
            node->impl = addr + 512;
            node->read = tar_read;
            node->write = 0;
            node->open = 0;
            node->close = 0;
            node->readdir = 0;
            node->finddir = 0;

            printf("[TAR] Registered file: %s (size: %d bytes)\n", node->name, node->length);
        }

        uint32_t aligned_size = (size + 511) & ~511;
        addr += 512 + aligned_size;
    }
}
