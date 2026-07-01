#include "vfs.h"
#include <string.h>

fs_node_t* fs_root = 0;

uint32_t read_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (node && node->read) {
        return node->read(node, offset, size, buffer);
    }
    return 0;
}

static int get_next_token(const char* path, int start, char* token) {
    if (path[start] == '\0') return -1;
    while (path[start] == '/') {
        start++;
    }
    if (path[start] == '\0') return -1;
    
    int i = 0;
    while (path[start] != '/' && path[start] != '\0') {
        token[i++] = path[start++];
    }
    token[i] = '\0';
    return start;
}

fs_node_t* find_node_by_path(const char* path) {
    if (fs_root == 0) return 0;
    if (strcmp(path, "/") == 0) return fs_root;

    fs_node_t* current = fs_root;
    char token[128];
    int offset = 0;

    while ((offset = get_next_token(path, offset, token)) != -1) {
        if (current->finddir == 0) return 0;
        fs_node_t* next = current->finddir(current, token);
        if (next == 0) return 0;
        current = next;
    }
    return current;
}
