# SLOP OS Hybrid Kernel Sketch

This note sketches the near-term hybrid-kernel design for two concrete paths:
- ELF program loading
- CD-ROM / ISO9660 file access

The goal is not a full microkernel split right away. The goal is to separate mechanism from policy so code can move out of kernel later without rewriting every caller.

## Design Rule

Keep in kernel:
- interrupt handling
- scheduler and process/task primitives
- privilege transitions
- low-level memory management
- low-level block I/O / ATAPI access
- syscall entry and the first IPC/request primitive

Move behind interfaces first:
- ELF loading policy
- filesystem policy
- mount policy
- service/process management helpers

Move to user space later if the boundary proves stable.

## Exec Path

Target flow for `exec("/bin/ls")`:

1. Shell or user process issues an exec request.
2. Kernel/VFS resolves the path.
3. `/bin` resolves to the CD-backed program tree.
4. Loader layer reads the ELF through VFS.
5. Loader validates ELF headers and builds a segment load plan.
6. Kernel creates the process object and user stack.
7. Kernel maps/copies segments into the user image region.
8. Kernel enters Ring 3 at the ELF entry point.

### Split of responsibility

Kernel mechanism:
- create process/task
- reserve image region / stack
- copy or map segments
- enter user mode
- tear down on failure

Loader policy:
- validate ELF headers
- reject unsupported file types
- choose segment layout rules
- build `argc/argv` startup layout
- define error reporting for exec failures

### Near-term interface

Keep the implementation in kernel for now, but force it through a loader boundary:

```c
struct exec_load_plan {
    u32 entry;
    u32 image_base;
    u32 image_size;
    u32 segment_count;
    struct exec_segment {
        u32 file_offset;
        u32 file_size;
        u32 mem_size;
        u32 vaddr;
        u32 flags;
    } segments[8];
};

int loader_prepare_exec(struct vfs_node *cwd, const char *path,
                        int argc, char **argv,
                        struct exec_load_plan *out_plan,
                        enum console_target target);

int proc_start_exec_plan(const struct exec_load_plan *plan,
                         int argc, char **argv,
                         struct exec_context *ctx);
```

### Why this helps

- The loader becomes policy, not process machinery.
- `loader_prepare_exec()` can later become a user-space loader service.
- `proc_start_exec_plan()` remains a small privileged kernel mechanism.

## CD-ROM Path

Target flow for `open("/mount/cdrom/foo")`:

1. User process issues `open`.
2. Syscall layer forwards to VFS.
3. VFS identifies the mount as ISO9660 on the CD device.
4. ISO layer resolves the path into a directory record / extent.
5. ISO layer requests sectors from the block layer.
6. Block layer calls the ATAPI device driver.
7. Data returns back up through block -> ISO -> VFS -> syscall layer.

### Split of responsibility

Kernel mechanism:
- ATAPI register access
- sector reads
- block-device registration
- mount table ownership
- VFS dispatch plumbing

Filesystem policy:
- ISO9660 path lookup
- filename normalization
- directory iteration semantics
- mount behavior rules
- future write policy for writable filesystems

### Near-term interface

Keep low-level device code in kernel. Put filesystem parsing behind a filesystem-ops boundary:

```c
struct fs_ops {
    int (*read_file)(int device_id, const char *path, char **out_data, u32 *out_size);
    int (*list_dir)(int device_id, const char *path, enum console_target target);
    int (*path_is_dir)(int device_id, const char *path);
    int (*list_dir_entries)(int device_id, const char *path,
                            struct fs_dir_entry *entries, u32 max_entries, u32 *out_count);
};
```

Then mount entries point to `fs_ops` rather than open-coded conditionals.

### Why this helps

- VFS stops caring whether the backing FS is in-kernel or not.
- ISO9660 becomes a replaceable module.
- A future FS service can implement the same contract through requests instead of direct calls.

## `/bin` Alias

Current practical model:
- `/bin` is an alias to `/mount/cdrom/bin`
- VFS handles the alias
- user tools remain unaware of the backing storage

That is fine for a hybrid kernel. The important part is that the alias logic stays in one VFS boundary rather than being duplicated in shell and exec code.

## Day 5 Implementation Target

Tomorrow's goal should be modest and structural:

1. Formalize the loader boundary.
2. Formalize filesystem operation tables.
3. Remove direct subsystem reach-throughs where possible.
4. Keep actual execution and storage mechanisms in kernel for now.
5. Leave clean seams for later user-space services.

## Concrete Tasks

### Exec / loader
- Introduce `exec_load_plan`.
- Move ELF parsing logic behind `loader_prepare_exec()`.
- Keep process start and Ring 3 entry in kernel.
- Make shell and future callers use one exec path.

### Filesystem
- Introduce `fs_ops` dispatch per mount.
- Move ISO9660 logic behind that interface.
- Keep ATAPI/block-device code unchanged for now.
- Keep `/bin` alias resolution in VFS, not in userland.

### Follow-up after that
- Add a small request/response IPC primitive.
- Convert one policy layer into a service-style boundary.
- Best first candidate: filesystem policy, not low-level block I/O.

## Non-Goals For Tomorrow

- Full microkernel conversion
- moving all drivers to user space
- replacing the syscall ABI
- building a real VM subsystem
- supporting multiple isolated user address spaces at once

Those can come later. Tomorrow should focus on architecture seams, not maximal churn.
