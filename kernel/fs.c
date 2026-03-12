#include "kernel.h"

#define MAX_MOUNTS 8
#define ISO_SECTOR_SIZE 2048u
#define ISO_FILE_MAX (2u * 1024u * 1024u)

struct iso_dir_loc {
    u32 extent_lba;
    u32 size;
    u8 flags;
};

static struct mount_entry mounts[MAX_MOUNTS];
static u32 mounts_used = 0;
/* Scratch sector buffer for ISO parsing/file reads to avoid deep stack use
   during nested lookup paths from Ring 3 syscalls. */
static u8 iso_sector_scratch[ISO_SECTOR_SIZE];

static int is_known_fs(const char *fs_name) {
    return str_eq(fs_name, "memfs") || str_eq(fs_name, "iso9660");
}

static void mounts_reset(void) {
    u32 i;
    mounts_used = 0;
    for (i = 0; i < MAX_MOUNTS; ++i) {
        mounts[i].used = 0;
        mounts[i].path[0] = '\0';
        mounts[i].fs_name[0] = '\0';
        mounts[i].device_id = -1;
    }
}

static const struct mount_entry *find_mount(const char *path) {
    u32 i;
    if (!path) {
        return (const struct mount_entry *)0;
    }
    for (i = 0; i < mounts_used; ++i) {
        if (mounts[i].used && str_eq(mounts[i].path, path)) {
            return &mounts[i];
        }
    }
    return (const struct mount_entry *)0;
}

static u32 read_u32_le(const u8 *p) {
    return ((u32)p[0]) |
           ((u32)p[1] << 8) |
           ((u32)p[2] << 16) |
           ((u32)p[3] << 24);
}

static char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static void fs_print_list_entry(enum console_target target, const char *name, int is_dir) {
    u8 fg;
    u8 bg;

    if (is_dir) {
        term_get_color(&fg, &bg);
        term_set_color(9, bg);
        console_printf(target, "%s/\n", name);
        term_set_color(fg, bg);
        return;
    }

    console_printf(target, "%s\n", name);
}

static int iso_name_matches(const u8 *rec_name, u32 rec_name_len, const char *component) {
    u32 i = 0;
    u32 j = 0;
    u32 effective_len = 0;

    while (effective_len < rec_name_len && rec_name[effective_len] != ';') {
        ++effective_len;
    }
    if (effective_len > 0u && rec_name[effective_len - 1u] == '.') {
        --effective_len;
    }

    while (i < effective_len) {
        char a = ascii_lower((char)rec_name[i]);
        char b = ascii_lower(component[j]);
        if (b == '\0' || a != b) {
            return 0;
        }
        ++i;
        ++j;
    }
    return component[j] == '\0';
}

static u32 iso_name_copy(const u8 *rec_name, u32 rec_name_len, char *out, u32 out_size) {
    u32 i = 0;
    u32 o = 0;
    u32 effective_len = 0;
    if (!out || out_size == 0u) {
        return 0u;
    }

    while (effective_len < rec_name_len && rec_name[effective_len] != ';') {
        ++effective_len;
    }
    if (effective_len > 0u && rec_name[effective_len - 1u] == '.') {
        --effective_len;
    }

    while (i < effective_len) {
        char c = (char)rec_name[i++];
        if (o + 1u >= out_size) {
            break;
        }
        out[o++] = ascii_lower(c);
    }
    out[o] = '\0';
    return o;
}

static int iso_read_pvd_root(int device_id, struct iso_dir_loc *out_root, u32 *out_volume_sectors) {
    u8 *sector = iso_sector_scratch;
    const u8 *root;
    if (!out_root || !storage_read(device_id, 16u, 1u, sector)) {
        return 0;
    }
    if (sector[0] != 1u || sector[6] != 1u ||
        sector[1] != 'C' || sector[2] != 'D' || sector[3] != '0' ||
        sector[4] != '0' || sector[5] != '1') {
        return 0;
    }

    if (out_volume_sectors) {
        *out_volume_sectors = read_u32_le(&sector[80]);
    }

    root = &sector[156];
    if (root[0] < 34u) {
        return 0;
    }
    out_root->extent_lba = read_u32_le(&root[2]);
    out_root->size = read_u32_le(&root[10]);
    out_root->flags = root[25];
    return 1;
}

static int iso_extent_valid(u32 extent_lba, u32 size_bytes, u32 volume_sectors) {
    u32 sectors = (size_bytes + (ISO_SECTOR_SIZE - 1u)) / ISO_SECTOR_SIZE;
    if (volume_sectors == 0u) {
        return 0;
    }
    if (extent_lba >= volume_sectors) {
        return 0;
    }
    if (sectors == 0u) {
        return 1;
    }
    if (extent_lba + sectors < extent_lba) {
        return 0;
    }
    return (extent_lba + sectors) <= volume_sectors;
}

static int iso_find_child(int device_id, const struct iso_dir_loc *dir, const char *name, struct iso_dir_loc *out_child) {
    u8 *sector = iso_sector_scratch;
    u32 offset = 0u;

    if (!dir || !name || !out_child) {
        return 0;
    }

    while (offset < dir->size) {
        u32 sec_off = offset % ISO_SECTOR_SIZE;
        u32 rec_len;
        const u8 *rec;
        u32 name_len;
        const u8 *rec_name;

        if (!storage_read(device_id, dir->extent_lba + (offset / ISO_SECTOR_SIZE), 1u, sector)) {
            return 0;
        }

        rec_len = sector[sec_off];
        if (rec_len == 0u) {
            offset = ((offset / ISO_SECTOR_SIZE) + 1u) * ISO_SECTOR_SIZE;
            continue;
        }
        if ((sec_off + rec_len) > ISO_SECTOR_SIZE || rec_len < 34u) {
            return 0;
        }

        rec = &sector[sec_off];
        name_len = rec[32];
        rec_name = &rec[33];

        if (!(name_len == 1u && (rec_name[0] == 0u || rec_name[0] == 1u))) {
            if (iso_name_matches(rec_name, name_len, name)) {
                out_child->extent_lba = read_u32_le(&rec[2]);
                out_child->size = read_u32_le(&rec[10]);
                out_child->flags = rec[25];
                return 1;
            }
        }

        offset += rec_len;
    }

    return 0;
}

static int iso_read_file_alloc(int device_id, const char *iso_path, char **out_data, u32 *out_size) {
    struct iso_dir_loc cur;
    struct iso_dir_loc child;
    u32 volume_sectors = 0u;
    const char *p = iso_path;
    char comp[64];
    u32 comp_len = 0u;
    int have_component = 0;
    char *buf;
    u32 total;
    u32 copied = 0u;
    u8 *sector = iso_sector_scratch;

    if (!iso_path || !out_data || !out_size) {
        return 0;
    }
    *out_data = (char *)0;
    *out_size = 0u;

    while (*p == '/') {
        ++p;
    }
    if (*p == '\0') {
        return 0;
    }

    if (!iso_read_pvd_root(device_id, &cur, &volume_sectors)) {
        return 0;
    }
    if (!iso_extent_valid(cur.extent_lba, cur.size, volume_sectors)) {
        return 0;
    }

    while (*p != '\0') {
        if (*p == '/') {
            if (comp_len == 0u) {
                ++p;
                continue;
            }
            comp[comp_len] = '\0';
            if (!iso_find_child(device_id, &cur, comp, &child) || (child.flags & 0x02u) == 0u) {
                return 0;
            }
            if (!iso_extent_valid(child.extent_lba, child.size, volume_sectors)) {
                return 0;
            }
            cur = child;
            comp_len = 0u;
            have_component = 1;
            ++p;
            continue;
        }
        if (comp_len + 1u >= sizeof(comp)) {
            return 0;
        }
        comp[comp_len++] = *p++;
        have_component = 1;
    }

    if (!have_component || comp_len == 0u) {
        return 0;
    }
    comp[comp_len] = '\0';
    if (!iso_find_child(device_id, &cur, comp, &child) || (child.flags & 0x02u) != 0u) {
        return 0;
    }
    if (!iso_extent_valid(child.extent_lba, child.size, volume_sectors)) {
        return 0;
    }
    if (child.size > ISO_FILE_MAX) {
        return 0;
    }

    buf = (char *)kmalloc(child.size + 1u);
    if (!buf) {
        return 0;
    }

    total = child.size;
    while (copied < total) {
        u32 chunk = total - copied;
        u32 i;
        if (chunk > ISO_SECTOR_SIZE) {
            chunk = ISO_SECTOR_SIZE;
        }
        if (!storage_read(device_id, child.extent_lba + (copied / ISO_SECTOR_SIZE), 1u, sector)) {
            return 0;
        }
        for (i = 0u; i < chunk; ++i) {
            buf[copied + i] = (char)sector[i];
        }
        copied += chunk;
    }
    buf[total] = '\0';

    *out_data = buf;
    *out_size = total;
    return 1;
}

static int iso_resolve_path(int device_id, const char *iso_path, struct iso_dir_loc *out_loc, int *out_is_dir) {
    struct iso_dir_loc cur;
    struct iso_dir_loc child;
    u32 volume_sectors = 0u;
    const char *p = iso_path;
    char comp[64];
    u32 comp_len = 0u;
    int have_component = 0;

    if (!iso_path || !out_loc || !out_is_dir) {
        return 0;
    }

    while (*p == '/') {
        ++p;
    }

    if (!iso_read_pvd_root(device_id, &cur, &volume_sectors)) {
        return 0;
    }
    if (!iso_extent_valid(cur.extent_lba, cur.size, volume_sectors)) {
        return 0;
    }

    if (*p == '\0') {
        *out_loc = cur;
        *out_is_dir = 1;
        return 1;
    }

    while (*p != '\0') {
        if (*p == '/') {
            if (comp_len == 0u) {
                ++p;
                continue;
            }
            comp[comp_len] = '\0';
            if (!iso_find_child(device_id, &cur, comp, &child) || (child.flags & 0x02u) == 0u) {
                return 0;
            }
            if (!iso_extent_valid(child.extent_lba, child.size, volume_sectors)) {
                return 0;
            }
            cur = child;
            comp_len = 0u;
            have_component = 1;
            ++p;
            continue;
        }
        if (comp_len + 1u >= sizeof(comp)) {
            return 0;
        }
        comp[comp_len++] = *p++;
        have_component = 1;
    }

    if (!have_component || comp_len == 0u) {
        *out_loc = cur;
        *out_is_dir = 1;
        return 1;
    }

    comp[comp_len] = '\0';
    if (!iso_find_child(device_id, &cur, comp, &child)) {
        return 0;
    }
    if (!iso_extent_valid(child.extent_lba, child.size, volume_sectors)) {
        return 0;
    }
    *out_loc = child;
    *out_is_dir = ((child.flags & 0x02u) != 0u);
    return 1;
}

static int iso_list_dir(int device_id, const struct iso_dir_loc *dir, enum console_target target) {
    u8 *sector = iso_sector_scratch;
    u32 offset = 0u;

    if (!dir) {
        return 0;
    }

    while (offset < dir->size) {
        u32 sec_off = offset % ISO_SECTOR_SIZE;
        u32 rec_len;
        const u8 *rec;
        u32 name_len;
        const u8 *rec_name;
        char name[64];

        if (!storage_read(device_id, dir->extent_lba + (offset / ISO_SECTOR_SIZE), 1u, sector)) {
            return 0;
        }

        rec_len = sector[sec_off];
        if (rec_len == 0u) {
            offset = ((offset / ISO_SECTOR_SIZE) + 1u) * ISO_SECTOR_SIZE;
            continue;
        }
        if ((sec_off + rec_len) > ISO_SECTOR_SIZE || rec_len < 34u) {
            return 0;
        }

        rec = &sector[sec_off];
        name_len = rec[32];
        rec_name = &rec[33];
        if (!(name_len == 1u && (rec_name[0] == 0u || rec_name[0] == 1u))) {
            (void)iso_name_copy(rec_name, name_len, name, sizeof(name));
            fs_print_list_entry(target, name, (rec[25] & 0x02u) != 0u);
        }

        offset += rec_len;
    }

    return 1;
}

static int iso_list_dir_entries(int device_id, const struct iso_dir_loc *dir, struct fs_dir_entry *entries, u32 max_entries, u32 *out_count) {
    u8 *sector = iso_sector_scratch;
    u32 offset = 0u;
    u32 count = 0u;

    if (!dir || !entries || !out_count) {
        return 0;
    }
    *out_count = 0u;

    while (offset < dir->size) {
        u32 sec_off = offset % ISO_SECTOR_SIZE;
        u32 rec_len;
        const u8 *rec;
        u32 name_len;
        const u8 *rec_name;

        if (!storage_read(device_id, dir->extent_lba + (offset / ISO_SECTOR_SIZE), 1u, sector)) {
            return 0;
        }

        rec_len = sector[sec_off];
        if (rec_len == 0u) {
            offset = ((offset / ISO_SECTOR_SIZE) + 1u) * ISO_SECTOR_SIZE;
            continue;
        }
        if ((sec_off + rec_len) > ISO_SECTOR_SIZE || rec_len < 34u) {
            return 0;
        }

        rec = &sector[sec_off];
        name_len = rec[32];
        rec_name = &rec[33];

        if (!(name_len == 1u && (rec_name[0] == 0u || rec_name[0] == 1u))) {
            if (count < max_entries) {
                struct fs_dir_entry *e = &entries[count];
                (void)iso_name_copy(rec_name, name_len, e->name, sizeof(e->name));
                e->is_dir = ((rec[25] & 0x02u) != 0u);
                ++count;
            }
        }

        offset += rec_len;
    }

    *out_count = count;
    return 1;
}

int fs_mount(const char *path, const char *fs_name, int device_id) {
    struct mount_entry *entry;

    if (!path || !fs_name || mounts_used >= MAX_MOUNTS || !is_known_fs(fs_name)) {
        return 0;
    }
    if (path[0] != '/') {
        return 0;
    }
    if (!str_eq(fs_name, "memfs") && !storage_get_device(device_id)) {
        return 0;
    }

    entry = &mounts[mounts_used];
    entry->used = 1;
    str_copy(entry->path, path, sizeof(entry->path));
    str_copy(entry->fs_name, fs_name, sizeof(entry->fs_name));
    entry->device_id = device_id;
    ++mounts_used;
    return 1;
}

const struct mount_entry *fs_mounts(void) {
    return mounts;
}

u32 fs_mount_count(void) {
    return mounts_used;
}

void fs_print_mounts(enum console_target target) {
    u32 i;

    if (mounts_used == 0u) {
        console_print(target, "(no mounts)\n");
        return;
    }

    for (i = 0; i < mounts_used; ++i) {
        const struct mount_entry *m = &mounts[i];
        console_printf(target, "%s on %s", m->fs_name, m->path);
        if (m->device_id >= 0) {
            const struct block_device *dev = storage_get_device(m->device_id);
            console_print(target, " dev=");
            if (dev) {
                console_print(target, dev->name);
            } else {
                console_print(target, "unknown");
            }
        }
        console_print(target, "\n");
    }
}

int fs_read_file_from_mount(const char *mount_path, const char *path, char **out_data, u32 *out_size) {
    const struct mount_entry *m = find_mount(mount_path);
    if (!m || !path || !out_data || !out_size || m->device_id < 0) {
        return 0;
    }
    if (str_eq(m->fs_name, "iso9660")) {
        return iso_read_file_alloc(m->device_id, path, out_data, out_size);
    }
    return 0;
}

int fs_list_dir_from_mount(const char *mount_path, const char *path, enum console_target target) {
    const struct mount_entry *m = find_mount(mount_path);
    struct iso_dir_loc loc;
    int is_dir = 0;
    const char *iso_path = path ? path : "/";

    if (!m || m->device_id < 0) {
        return 0;
    }
    if (!str_eq(m->fs_name, "iso9660")) {
        return 0;
    }
    if (!iso_resolve_path(m->device_id, iso_path, &loc, &is_dir) || !is_dir) {
        return 0;
    }
    return iso_list_dir(m->device_id, &loc, target);
}

int fs_path_is_dir_from_mount(const char *mount_path, const char *path) {
    const struct mount_entry *m = find_mount(mount_path);
    struct iso_dir_loc loc;
    int is_dir = 0;
    const char *iso_path = path ? path : "/";

    if (!m || m->device_id < 0) {
        return 0;
    }
    if (!str_eq(m->fs_name, "iso9660")) {
        return 0;
    }
    if (!iso_resolve_path(m->device_id, iso_path, &loc, &is_dir)) {
        return 0;
    }
    return is_dir;
}

int fs_list_dir_entries_from_mount(const char *mount_path, const char *path, struct fs_dir_entry *entries, u32 max_entries, u32 *out_count) {
    const struct mount_entry *m = find_mount(mount_path);
    struct iso_dir_loc loc;
    int is_dir = 0;
    const char *iso_path = path ? path : "/";

    if (!m || m->device_id < 0 || !entries || max_entries == 0u || !out_count) {
        return 0;
    }
    if (!str_eq(m->fs_name, "iso9660")) {
        return 0;
    }
    if (!iso_resolve_path(m->device_id, iso_path, &loc, &is_dir) || !is_dir) {
        return 0;
    }
    return iso_list_dir_entries(m->device_id, &loc, entries, max_entries, out_count);
}

void fs_init(void) {
    const struct block_device *cd0;

    mounts_reset();
    fs_mount("/", "memfs", -1);

    cd0 = storage_find_device("cd0");
    if (cd0) {
        fs_mount("/mount/cdrom", "iso9660", cd0->id);
    }
}
