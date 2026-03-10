#include "kernel.h"

#define VFS_NAME_MAX 31
#define VFS_PATH_MAX 128

enum vfs_node_type {
    VFS_NODE_DIR = 1,
    VFS_NODE_FILE = 2
};

struct vfs_node {
    char name[VFS_NAME_MAX + 1];
    u32 type;
    struct vfs_node *parent;
    struct vfs_node *first_child;
    struct vfs_node *next_sibling;
    char *data;
    u32 size;
};

static struct vfs_node *vfs_root_node = (struct vfs_node *)0;

static struct vfs_node *vfs_resolve_from(struct vfs_node *cwd, const char *path);

static struct vfs_node *vfs_alloc_node(const char *name, u32 type, struct vfs_node *parent) {
    struct vfs_node *node = (struct vfs_node *)kcalloc(sizeof(struct vfs_node));
    if (!node) {
        return (struct vfs_node *)0;
    }

    str_copy(node->name, name, sizeof(node->name));
    node->type = type;
    node->parent = parent;

    if (parent) {
        node->next_sibling = parent->first_child;
        parent->first_child = node;
    }

    return node;
}

static struct vfs_node *vfs_find_child(struct vfs_node *dir, const char *name) {
    struct vfs_node *child = dir ? dir->first_child : (struct vfs_node *)0;
    while (child) {
        if (str_eq(child->name, name)) {
            return child;
        }
        child = child->next_sibling;
    }
    return (struct vfs_node *)0;
}

static int vfs_next_component(const char **path, char *name, size_t size) {
    u32 i = 0;
    const char *p = *path;

    while (*p == '/') {
        ++p;
    }
    if (*p == '\0') {
        *path = p;
        return 0;
    }

    while (*p && *p != '/') {
        if (i + 1u >= size) {
            return -1;
        }
        name[i++] = *p++;
    }
    name[i] = '\0';
    *path = p;
    return 1;
}

static struct vfs_node *vfs_resolve(const char *path) {
    return vfs_resolve_from(vfs_root_node, path);
}

static struct vfs_node *vfs_resolve_from(struct vfs_node *cwd, const char *path) {
    struct vfs_node *node;
    char component[VFS_NAME_MAX + 1];
    int status;

    if (!path || *path == '\0') {
        return cwd;
    }

    node = (path[0] == '/') ? vfs_root_node : cwd;
    while (*path == '/') {
        ++path;
    }

    if (*path == '\0') {
        return node;
    }

    for (;;) {
        status = vfs_next_component(&path, component, sizeof(component));
        if (status <= 0) {
            return status == 0 ? node : (struct vfs_node *)0;
        }

        if (str_eq(component, ".")) {
        } else if (str_eq(component, "..")) {
            if (node->parent) {
                node = node->parent;
            }
        } else {
            node = vfs_find_child(node, component);
            if (!node) {
                return (struct vfs_node *)0;
            }
        }
    }
}

static int vfs_split_parent(const char *path, char *parent_path, size_t parent_size, char *name, size_t name_size) {
    const char *last = path;
    const char *scan = path;
    u32 parent_len;
    u32 name_len;

    if (!path || *path == '\0') {
        return 0;
    }

    while (*scan) {
        if (*scan == '/') {
            last = scan;
        }
        ++scan;
    }

    if (last == path && *last != '/') {
        str_copy(parent_path, "", parent_size);
        str_copy(name, path, name_size);
        return name[0] != '\0';
    }

    if (*last == '/') {
        parent_len = (u32)(last - path);
        if (parent_len == 0u) {
            str_copy(parent_path, "/", parent_size);
        } else {
            if (parent_len + 1u > parent_size) {
                return 0;
            }
            {
                u32 i;
                for (i = 0; i < parent_len; ++i) {
                    parent_path[i] = path[i];
                }
                parent_path[parent_len] = '\0';
            }
        }

        ++last;
        name_len = str_len(last);
        if (name_len + 1u > name_size) {
            return 0;
        }
        str_copy(name, last, name_size);
    } else {
        return 0;
    }

    if (name[0] == '\0' || str_eq(name, ".") || str_eq(name, "..")) {
        return 0;
    }

    return 1;
}

static int vfs_write_seed_file(const char *path, const char *data) {
    struct vfs_node *file;
    u32 size = str_len(data);
    char *copy;
    u32 i;

    if (!vfs_touch(vfs_root_node, path)) {
        return 0;
    }

    file = vfs_resolve(path);
    if (!file || file->type != VFS_NODE_FILE) {
        return 0;
    }

    copy = (char *)kmalloc(size + 1u);
    if (!copy) {
        return 0;
    }

    for (i = 0; i < size; ++i) {
        copy[i] = data[i];
    }
    copy[size] = '\0';
    file->data = copy;
    file->size = size;
    return 1;
}

void vfs_init(void) {
    if (vfs_root_node) {
        return;
    }

    vfs_root_node = vfs_alloc_node("", VFS_NODE_DIR, (struct vfs_node *)0);

    vfs_make_dir(vfs_root_node, "/bin");
    vfs_make_dir(vfs_root_node, "/docs");
    vfs_make_dir(vfs_root_node, "/tmp");
    vfs_write_seed_file("/docs/readme.txt",
        "SLOP in-memory VFS\n"
        "This is not a disk-backed filesystem yet.\n");
    vfs_write_seed_file("/docs/roadmap.txt",
        "Next: more memory management, a better VFS, and real userland.\n");
}

struct vfs_node *vfs_root(void) {
    return vfs_root_node;
}

int vfs_change_dir(struct vfs_node **cwd, const char *path) {
    struct vfs_node *node = vfs_resolve_from(*cwd, path);
    if (!node || node->type != VFS_NODE_DIR) {
        return 0;
    }
    *cwd = node;
    return 1;
}

int vfs_make_dir(struct vfs_node *cwd, const char *path) {
    char parent_path[VFS_PATH_MAX];
    char name[VFS_NAME_MAX + 1];
    struct vfs_node *parent;

    if (!vfs_split_parent(path, parent_path, sizeof(parent_path), name, sizeof(name))) {
        return 0;
    }

    parent = (*parent_path == '\0') ? cwd : vfs_resolve_from(cwd, parent_path);
    if (!parent || parent->type != VFS_NODE_DIR || vfs_find_child(parent, name)) {
        return 0;
    }

    return vfs_alloc_node(name, VFS_NODE_DIR, parent) != (struct vfs_node *)0;
}

int vfs_touch(struct vfs_node *cwd, const char *path) {
    char parent_path[VFS_PATH_MAX];
    char name[VFS_NAME_MAX + 1];
    struct vfs_node *parent;
    struct vfs_node *node;

    if (!vfs_split_parent(path, parent_path, sizeof(parent_path), name, sizeof(name))) {
        return 0;
    }

    parent = (*parent_path == '\0') ? cwd : vfs_resolve_from(cwd, parent_path);
    if (!parent || parent->type != VFS_NODE_DIR) {
        return 0;
    }

    node = vfs_find_child(parent, name);
    if (node) {
        return node->type == VFS_NODE_FILE;
    }

    return vfs_alloc_node(name, VFS_NODE_FILE, parent) != (struct vfs_node *)0;
}

void vfs_list(struct vfs_node *cwd, const char *path, enum console_target target) {
    struct vfs_node *node = vfs_resolve_from(cwd, path);
    struct vfs_node *child;

    if (!node || node->type != VFS_NODE_DIR) {
        console_print(target, "ls: no such directory\n");
        return;
    }

    child = node->first_child;
    while (child) {
        if (child->type == VFS_NODE_DIR) {
            console_printf(target, "%s/\n", child->name);
        } else {
            console_printf(target, "%s\n", child->name);
        }
        child = child->next_sibling;
    }
}

int vfs_read_file(struct vfs_node *cwd, const char *path, const char **data, u32 *size) {
    struct vfs_node *node = vfs_resolve_from(cwd, path);
    if (!node || node->type != VFS_NODE_FILE) {
        return 0;
    }

    *data = node->data;
    *size = node->size;
    return 1;
}

int vfs_write_file(struct vfs_node *cwd, const char *path, const char *data, u32 size) {
    struct vfs_node *node;
    char *copy;
    u32 i;

    if (!vfs_touch(cwd, path)) {
        return 0;
    }

    node = vfs_resolve_from(cwd, path);
    if (!node || node->type != VFS_NODE_FILE) {
        return 0;
    }

    copy = (char *)kmalloc(size + 1u);
    if (!copy) {
        return 0;
    }

    for (i = 0; i < size; ++i) {
        copy[i] = data[i];
    }
    copy[size] = '\0';

    node->data = copy;
    node->size = size;
    return 1;
}

void vfs_get_cwd_path(struct vfs_node *cwd, char *buf, size_t size) {
    const char *parts[16];
    u32 count = 0;
    u32 i;
    size_t remaining = size;
    char *out = buf;
    struct vfs_node *node = cwd;

    if (size == 0u) {
        return;
    }

    if (node == vfs_root_node) {
        str_copy(buf, "/", size);
        return;
    }

    while (node && node != vfs_root_node && count < 16u) {
        parts[count++] = node->name;
        node = node->parent;
    }

    if (remaining > 1u) {
        *out++ = '/';
        --remaining;
    }

    for (i = count; i > 0u; --i) {
        u32 len = str_len(parts[i - 1u]);
        u32 j;
        if (len + 1u > remaining) {
            break;
        }
        for (j = 0; j < len; ++j) {
            *out++ = parts[i - 1u][j];
        }
        remaining = (size_t)(remaining - len);
        if (i > 1u) {
            *out++ = '/';
            --remaining;
        }
    }

    *out = '\0';
}
