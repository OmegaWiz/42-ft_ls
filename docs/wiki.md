# ft_ls — Allowed Functions Wiki

Reference for every function permitted in the mandatory part of `ft_ls`.
Assumes solid C knowledge; focuses on what matters for this project specifically.

---

## Table of Contents

0. [Concepts — Inodes & Symbolic links](#0-concepts--inodes--symbolic-links)
1. [Directory traversal — `opendir`, `readdir`, `closedir`](#1-directory-traversal)
2. [File metadata — `stat`, `lstat`](#2-file-metadata)
3. [User & group names — `getpwuid`, `getgrgid`](#3-user--group-names)
4. [Extended attributes — `listxattr`, `getxattr`](#4-extended-attributes)
5. [Time — `time`, `ctime`](#5-time)
6. [Symbolic links — `readlink`](#6-symbolic-links)
7. [Memory — `malloc`, `free`](#7-memory)
8. [Error reporting — `perror`, `strerror`](#8-error-reporting)
9. [Output — `write`](#9-output)
10. [Exit — `exit`](#10-exit)

---

## 0. Concepts — Inodes & Symbolic links

Understanding inodes and symlinks is essential before reading any of the
function entries — they explain *why* `stat` and `lstat` differ, why
`st_nlink` exists, and why `-R` must be written carefully.

---

### Inodes

The filesystem stores every file as two separate things:

1. **The data** — the raw bytes on disk.
2. **The inode** — a fixed-size metadata record stored separately, identified
   by a unique integer (`st_ino`).

The inode holds everything *about* the file except its name:

```
inode #42
├── type + permissions      (st_mode)
├── owner uid / gid         (st_uid, st_gid)
├── size in bytes           (st_size)
├── timestamps              (st_atimespec, st_mtimespec, st_ctimespec)
├── block addresses         (where on disk the data actually lives)
└── hard link count         (st_nlink)
```

**The filename lives in the directory, not in the inode.** A directory is
essentially a table that maps names to inode numbers:

```
/home/user/
├── "foo.txt"  →  inode #42
├── "bar.txt"  →  inode #17
└── "docs"     →  inode #99   (a subdirectory — has its own inode)
```

---

### Hard links

Because names and inodes are decoupled, multiple names can point to the
**same inode**:

```
"original.txt"  →  inode #42
"alias.txt"     →  inode #42   ← same inode, same data
```

This is a hard link. `st_nlink` is the count of how many names currently
point to an inode. When `st_nlink` reaches `0`, the kernel frees the inode
and the disk blocks. This is why the syscall is called `unlink`, not
`delete` — it removes a name, not necessarily the data.

Normal files start with `st_nlink = 1`. Each hard link added increments it.
Directories always start at `2` (the entry in their parent plus the `.` entry
inside themselves); each subdirectory adds another `1` via its `..` entry.

---

### Symbolic links

A symlink is a **file whose data content is a path string**. It has its own
inode with type `S_IFLNK`, and `st_size` equals the length of that path
string in bytes.

```
"shortcut"  →  inode #77
               st_mode: S_IFLNK
               st_size: 18
               data:    "/home/user/foo.txt"   ← just a string
```

When the kernel opens `"shortcut"`, it reads that string and restarts the
path lookup from it. The symlink has **no connection** to the target's inode —
it only stores the text. If the target is moved or deleted, the symlink
becomes dangling but is otherwise unchanged.

---

### `stat` vs `lstat`

This is the practical consequence of the above:

```
shortcut  →  /home/user/foo.txt  (inode #42)

stat("shortcut")   → follows the path string → reports on inode #42 (the target)
lstat("shortcut")  → stops at the link itself → reports on inode #77 (the link)
```

`ls -l` uses `lstat` everywhere so symlinks appear as type `l` and show their
own size. It then calls `readlink` to retrieve the stored path string and
prints it as the `-> target` suffix.

---

### Hard link vs symlink — comparison

| | Hard link | Symbolic link |
|---|---|---|
| Points to | inode directly | a path string |
| Cross-filesystem | No | Yes |
| Target deleted | Data survives (`st_nlink > 0`) | Link becomes dangling |
| `st_nlink` incremented | Yes | No |
| Type shown in `ls -l` | same as the target | `l` |
| Can link to directories | Not normally allowed | Yes |

---

### Relevance to ft_ls

| `struct stat` field | How it affects ft_ls |
|---|---|
| `st_nlink` | Printed as the link count column in `-l` |
| `st_ino` | Not printed by default `ls`, but is the inode number underlying everything |
| `st_mode` with `S_ISLNK` | Triggers `readlink` call and `-> target` display |
| `st_size` on a symlink | Length of the target path string, not the target file's size |

**`-R` and symlinks:** when recursing, only descend into entries where
`S_ISDIR(st.st_mode)` is true after an `lstat` call. If you mistakenly used
`stat`, a symlink pointing to a directory would look like a directory and
cause unintended (potentially infinite) recursion.

---

### Path resolution

When the kernel resolves a path like `/home/user/docs/file.txt`, it walks each
component one directory lookup at a time:

```
/          →  inode #2   (root directory)
home       →  inode #5
user       →  inode #18
docs       →  inode #99
file.txt   →  inode #42
```

Each step is a directory read + inode fetch. Symlinks can appear at any
component; the kernel follows them transparently (up to a nesting limit,
typically 8–40 hops, after which `ELOOP` is returned).

**Relative paths** are resolved starting from the current working directory.
`"."` always refers to the current directory's own inode; `".."` refers to
its parent's inode. These are real directory entries, not special kernel magic
— you will see them in `readdir` output.

---

### The `total` line

When `ls -l` lists a directory, it prints a `total N` header before the
entries. `N` is the sum of `st_blocks` for every entry in the directory
(excluding `.` and `..`), divided by `2`, converted to 1 KB units.

```c
// st_blocks is in 512-byte units on both macOS and Linux
// ls -l displays in 1024-byte (1 KB) units
total_blocks += entry_stat.st_blocks;
// print: total_blocks / 2
```

This line is only printed when listing a directory, not when listing
individual file arguments directly.

---

### How `ls` orders arguments

Real `ls` processes its arguments in two passes:

1. **Files first** — non-directory arguments are listed immediately, sorted
   among themselves.
2. **Directories second** — each directory argument is listed, sorted among
   themselves. If multiple directories are listed, each gets a header line
   showing its path, and they are separated by a blank line.

Under `-R`, each subdirectory encountered is appended to the queue and
processed after its parent finishes.

---

### Column alignment in `-l` mode

Every column in `-l` output is right-aligned to the width of the widest value
in that column *for the current directory listing*. This means you must make
**two passes** over the entry list:

1. First pass — `lstat` every entry, record the widths of `st_nlink`,
   `st_size`, owner name, group name.
2. Second pass — print each line, padding each field to the maximum width
   found in pass one.

Mismatched padding is an automatic deduction at evaluation.

---

### Sorting rules

Default sort (no flags): **lexicographic by filename**, case-sensitive,
using raw byte values (`LC_ALL=C`). This is equivalent to `strcmp`.

| Flag | Sort key | Tie-break |
|------|----------|-----------|
| (none) | `d_name` lexicographic | — |
| `-t` | `st_mtimespec.tv_sec` descending (newest first) | `d_name` lexicographic |
| `-r` | reverses whatever order is active | — |
| `-a` | includes dot-files in the same sort | — |

Under `-t`, files with identical modification seconds should fall back to
name order, not leave them in `readdir` order.

---

### File descriptor budget

Every open directory consumes one file descriptor. Under `-R`, you hold one
`DIR *` open per level of nesting while recursing. The default per-process
limit (`OPEN_MAX`) is 256 on macOS. On a deep tree this can be a problem.

The safe approach: fully read all entries from a directory into memory, call
`closedir`, *then* recurse into subdirectories. This keeps at most one `DIR *`
open at a time regardless of depth.

---

### `errno` discipline

Several functions signal failure by returning `NULL` or `-1`, but they also
set `errno`. Rules to follow:

- Always check return values. Never assume success.
- Do not call any other function between the failing call and your error
  handler — any intervening call may overwrite `errno`.
- When distinguishing "not found" from "error" (e.g. `getpwuid` returning
  `NULL`), set `errno = 0` **before** the call, then check it after.
- `readdir` returning `NULL` means either end-of-directory (`errno == 0`) or
  a read error (`errno != 0`). Always reset `errno = 0` before each call.

---

### macOS vs Linux differences

> The 42 project targets macOS. However, if you test on Linux, these
> differences will cause subtle failures.

#### `struct stat` — timestamp field names

| macOS | Linux | Type |
|-------|-------|------|
| `st_atimespec` | `st_atim` | `struct timespec` |
| `st_mtimespec` | `st_mtim` | `struct timespec` |
| `st_ctimespec` | `st_ctim` | `struct timespec` |
| `st_birthtimespec` | *(does not exist)* | — |

On Linux, `st_mtime` is a macro for `st_mtim.tv_sec`, kept for POSIX
compatibility. On macOS, `st_mtime` is similarly a macro for
`st_mtimespec.tv_sec`. Using the macro `st_mtime` works on both, but
`st_mtimespec` will fail to compile on Linux.

Linux has no `st_birthtimespec` — creation time is not stored by most Linux
filesystems (ext4, xfs). Do not rely on it.

#### `struct stat` — block size reporting

`st_blocks` is always in **512-byte units** on both platforms. However, the
*values reported* for directories differ: macOS often reports `0` blocks for
empty directories; Linux typically reports `8` (one 4 KB page / 512 = 8).
Your `total` line will differ between platforms for the same tree.

#### `struct dirent` layout

The macOS 64-bit inode variant has `d_seekoff` (the second `uint64_t` field).
Linux's `struct dirent` has no such field — its layout is:

```c
// Linux struct dirent (glibc)
struct dirent {
    ino_t          d_ino;     /* 64-bit inode number */
    off_t          d_off;     /* offset to next dirent (opaque) */
    unsigned short d_reclen;  /* length of this record */
    unsigned char  d_type;    /* file type */
    char           d_name[];  /* filename (flexible array, NAME_MAX+1 bytes) */
};
```

Never access `d_seekoff` or `d_off` directly — they are internal to the
implementation. Only `d_ino`, `d_type`, `d_namlen`/`d_reclen`, and `d_name`
are portable.

#### `d_type` reliability

On macOS (HFS+, APFS): `d_type` is always populated.
On Linux: `d_type` may be `DT_UNKNOWN` on ext2/ext3, network filesystems, and
some tmpfs configurations. Always fall back to `lstat` when `d_type` is
`DT_UNKNOWN`. Since you need `lstat` for `-l` output anyway, use `st_mode`
from `lstat` as the authoritative source of file type everywhere.

#### `getxattr` / `listxattr` signatures

macOS adds two extra parameters that Linux does not have:

```c
// macOS
ssize_t getxattr(const char *path, const char *name,
                 void *value, size_t size, u_int32_t position, int options);

// Linux
ssize_t getxattr(const char *path, const char *name,
                 void *value, size_t size);
```

Code using the macOS signature will not compile on Linux without a shim.

#### `DT_WHT` (whiteout)

macOS defines `DT_WHT` (value `14`) for union-filesystem whiteout entries.
Linux does not. You will not encounter whiteout entries in normal usage, but
do not assume the `d_type` value space ends at `DT_SOCK`.

#### `st_flags` (user-defined file flags)

`st_flags` in `struct stat` is macOS/BSD-specific (`chflags(2)`). It does not
exist on Linux. The field holds flags like `UF_IMMUTABLE` (user immutable),
`SF_ARCHIVED`, etc. Irrelevant for the mandatory part but present in the
struct.

#### `ls` output differences

Even the reference `ls` binary behaves differently between platforms:

| Behaviour | macOS `ls` | GNU `ls` (Linux) |
|-----------|-----------|-----------------|
| Default colour | off | often on via alias |
| Symlink in `-l` | `lrwxr-xr-x` | `lrwxrwxrwx` (permissions always `777`) |
| `total` units | 512-byte blocks / 2 = KB | same formula but reported values may differ |
| `@` xattr indicator | yes (built-in) | no (requires `-Z` for SELinux context) |
| ACL indicator `+` | yes | yes (different mechanism) |

Always use macOS's `/bin/ls` as the reference binary for your evaluation, not
GNU `ls`.

---

## 1. Directory traversal

### `opendir`

**Prototype**
```c
#include <dirent.h>
DIR *opendir(const char *filename);
```

**Overview**
Opens the directory named by `filename`, allocates a `DIR` stream object for
it, and returns a handle to that stream. The stream's internal cursor starts
before the first entry. Does not read any entries yet.

**Parameters**
- `filename` — Path to the directory to open. May be absolute or relative.

**Return value**
A valid `DIR *` on success. `NULL` on failure with `errno` set (common errors:
`ENOENT` — path doesn't exist; `ENOTDIR` — path is not a directory;
`EACCES` — no read permission).

**Notes**
- Use this to open each directory argument passed to `ft_ls`, and again
  recursively for every sub-directory encountered under `-R`.
- Every `opendir` must be paired with a `closedir`; each open directory
  consumes a file descriptor.

---

### `readdir`

**Prototype**
```c
#include <dirent.h>
struct dirent *readdir(DIR *dirp);
```

**Overview**
Advances the stream cursor and returns a pointer to the next directory entry.
The entries are not guaranteed to arrive in any particular order — `readdir`
does not sort.

**Parameters**
- `dirp` — A `DIR *` previously returned by `opendir`.

**Return value**
A pointer to a `struct dirent` describing the next entry, or `NULL` on both
end-of-directory and error. To tell them apart, set `errno = 0` before each
call and inspect it when `NULL` is returned: if `errno` is still `0`, the
directory is fully consumed; if `errno != 0`, a read error occurred.

**Notes**
- The returned pointer is **only valid until the next call to `readdir` or
  `closedir` on the same stream**. Copy `d_name` (e.g. `ft_strdup`) if you
  need to keep the name.
- `readdir` always yields `.` and `..`. Without `-a`, skip any entry whose
  `d_name[0] == '.'`.
- Collect all entries into an array first, then sort — you cannot seek
  backwards in a `DIR` stream.

#### `struct dirent` (macOS, 64-bit inode variant)

```c
struct dirent {
    uint64_t   d_ino;               /* inode number of the entry */
    uint64_t   d_seekoff;           /* seek offset for servers (optional, rarely used) */
    uint16_t   d_reclen;            /* total length of this record in bytes */
    uint16_t   d_namlen;            /* length of the string in d_name (not counting NUL) */
    uint8_t    d_type;              /* file type — see DT_* constants below */
    char       d_name[1024];        /* entry name, always NUL-terminated */
};
```

#### `d_type` constants

| Constant     | Meaning                                    |
|--------------|--------------------------------------------|
| `DT_REG`     | Regular file                               |
| `DT_DIR`     | Directory                                  |
| `DT_LNK`     | Symbolic link                              |
| `DT_CHR`     | Character device                           |
| `DT_BLK`     | Block device                               |
| `DT_FIFO`    | Named pipe (FIFO)                          |
| `DT_SOCK`    | Socket                                     |
| `DT_UNKNOWN` | Type unknown — must call `lstat` to determine |

> Some filesystems (e.g. network mounts) always report `DT_UNKNOWN`. Never
> rely on `d_type` alone; you will be calling `lstat` on every entry for `-l`
> output anyway, so use `st_mode` from that for type checks.

---

### `closedir`

**Prototype**
```c
#include <dirent.h>
int closedir(DIR *dirp);
```

**Overview**
Closes the directory stream, releases its internal buffer, and closes the
underlying file descriptor.

**Parameters**
- `dirp` — The `DIR *` handle to close. Must not be used after this call.

**Return value**
`0` on success, `-1` on failure with `errno` set.

**Notes**
- Always call this after you finish iterating, including on error paths.
  Forgetting it leaks a file descriptor and will eventually cause `opendir` to
  fail with `EMFILE`.

---

### Typical traversal skeleton

```c
DIR           *dir;
struct dirent *entry;

dir = opendir(path);
if (!dir)
{
    perror(path);
    return ;
}
errno = 0;
while ((entry = readdir(dir)) != NULL)
{
    // copy entry->d_name into your list
    errno = 0;
}
if (errno != 0)
    perror(path);   // readdir failed mid-stream
closedir(dir);
```

---

## 2. File metadata

### `stat`

**Prototype**
```c
#include <sys/stat.h>
int stat(const char *restrict path, struct stat *restrict buf);
```

**Overview**
Retrieves metadata about the file at `path` and writes it into `buf`.
**Follows symbolic links** — if `path` is a symlink, `stat` reports on the
symlink's target, not the link itself.

**Parameters**
- `path` — Path to the file or directory to inspect.
- `buf` — Caller-allocated `struct stat` to fill in.

**Return value**
`0` on success, `-1` on failure with `errno` set (common errors: `ENOENT`,
`EACCES`, `ENOTDIR`).

**Notes**
- Because `stat` dereferences symlinks, prefer `lstat` for most operations in
  `ft_ls`. Use `stat` only when you explicitly want to inspect the target (e.g.
  checking whether a symlink's destination is a directory for `-R`).

---

### `lstat`

**Prototype**
```c
#include <sys/stat.h>
int lstat(const char *restrict path, struct stat *restrict buf);
```

**Overview**
Identical to `stat` except it **does not follow the final symlink**: if `path`
names a symlink, `buf` is filled with information about the link itself
(type `S_IFLNK`, size = length of the target path string, etc.).

**Parameters**
- `path` — Path to the file, directory, or symlink to inspect.
- `buf` — Caller-allocated `struct stat` to fill in.

**Return value**
`0` on success, `-1` on failure with `errno` set.

**Notes**
- Use `lstat` as the default call in `ft_ls`. It ensures symlinks display
  as type `l` in `-l` output instead of showing the target's type.

---

### `struct stat` (macOS, 64-bit inode)

```c
struct stat {
    dev_t           st_dev;           /* ID of device containing file */
    mode_t          st_mode;          /* file type + permission bits */
    nlink_t         st_nlink;         /* number of hard links */
    ino_t           st_ino;           /* inode number */
    uid_t           st_uid;           /* owner user ID */
    gid_t           st_gid;           /* owner group ID */
    dev_t           st_rdev;          /* device ID (char/block devices only) */
    struct timespec st_atimespec;     /* last access time */
    struct timespec st_mtimespec;     /* last data modification time */
    struct timespec st_ctimespec;     /* last status-change time */
    struct timespec st_birthtimespec; /* creation (birth) time */
    off_t           st_size;          /* file size in bytes */
    blkcnt_t        st_blocks;        /* blocks allocated (512-byte units) */
    blksize_t       st_blksize;       /* optimal I/O block size */
    uint32_t        st_flags;         /* user-defined flags (chflags) */
    uint32_t        st_gen;           /* file generation number */
};
```

#### Fields used in ft_ls

| Field | Used for |
|-------|----------|
| `st_mode` | File type character and permission string (`-rwxr-xr-x`) |
| `st_nlink` | Link count column in `-l` |
| `st_uid` | Passed to `getpwuid` to resolve owner name |
| `st_gid` | Passed to `getgrgid` to resolve group name |
| `st_size` | File size column in `-l` |
| `st_blocks` | Summed for the `total N` line (512-byte units; `ls -l` shows 1 KB blocks, so sum / 2) |
| `st_mtimespec.tv_sec` | Sort key for `-t`; timestamp for `-l` display |
| `st_rdev` | Major/minor device numbers for device files (bonus) |

#### `st_mode` dissection

```c
/* File type — test with these macros or mask with S_IFMT */
S_ISREG(m)   /* regular file       → '-' */
S_ISDIR(m)   /* directory          → 'd' */
S_ISLNK(m)   /* symbolic link      → 'l' */
S_ISCHR(m)   /* character device   → 'c' */
S_ISBLK(m)   /* block device       → 'b' */
S_ISFIFO(m)  /* named pipe         → 'p' */
S_ISSOCK(m)  /* socket             → 's' */

/* Permission bits */
S_ISUID  0004000   /* setuid  — 'x' becomes 's'/'S' for owner */
S_ISGID  0002000   /* setgid  — 'x' becomes 's'/'S' for group */
S_ISVTX  0001000   /* sticky  — 'x' becomes 't'/'T' for other */
S_IRUSR  0000400   S_IWUSR  0000200   S_IXUSR  0000100  /* owner */
S_IRGRP  0000040   S_IWGRP  0000020   S_IXGRP  0000010  /* group */
S_IROTH  0000004   S_IWOTH  0000002   S_IXOTH  0000001  /* other */
```

**Building the permission string** (`-rwxr-xr-x`):
1. First character: file type from the macros above.
2. Three triads (owner / group / other): test each bit → print letter or `-`.
3. Special execute characters: if setuid/setgid is set and the execute bit is
   also set → `s`; setuid/setgid set but no execute → `S`. Sticky + other
   execute → `t`; sticky without other execute → `T`.

---

## 3. User & group names

### `getpwuid`

**Prototype**
```c
#include <pwd.h>
struct passwd *getpwuid(uid_t uid);
```

**Overview**
Looks up the system user database (ultimately `/etc/passwd` / Directory
Services) for the entry matching `uid` and returns a pointer to a struct
containing that user's information.

**Parameters**
- `uid` — The numeric user ID to look up (from `st_uid`).

**Return value**
Pointer to a static, thread-local `struct passwd` on success. `NULL` if no
matching user is found, or on error (set `errno = 0` before calling if you
need to distinguish the two).

**Notes**
- The returned pointer is overwritten by the next call to `getpwuid` on the
  same thread. Copy `pw_name` with `ft_strdup` if you store it.
- **Fallback:** when `NULL` is returned, `ls` prints the raw numeric UID as a
  decimal string. Replicate this.

#### `struct passwd` (relevant field)

```c
struct passwd {
    char  *pw_name;  /* login name — printed by ls -l */
    uid_t  pw_uid;
    gid_t  pw_gid;
    /* ... other fields not needed */
};
```

---

### `getgrgid`

**Prototype**
```c
#include <grp.h>
struct group *getgrgid(gid_t gid);
```

**Overview**
Looks up the system group database for the entry matching `gid` and returns a
pointer to a struct containing that group's information.

**Parameters**
- `gid` — The numeric group ID to look up (from `st_gid`).

**Return value**
Pointer to a static, thread-local `struct group` on success. `NULL` if no
matching group is found or on error. Same `errno` caveat as `getpwuid`.

**Notes**
- Same static-buffer lifetime as `getpwuid`. Copy `gr_name` if you store it.
- **Fallback:** when `NULL`, print the raw numeric GID.

#### `struct group` (relevant field)

```c
struct group {
    char  *gr_name;  /* group name — printed by ls -l */
    gid_t  gr_gid;
    /* ... */
};
```

---

## 4. Extended attributes

> Strictly required only for the **bonus** (the `@` indicator and ACL
> display), but both functions are on the allowed list.

### `listxattr`

**Prototype**
```c
#include <sys/xattr.h>
ssize_t listxattr(const char *path, char *namebuf, size_t size, int options);
```

**Overview**
Returns the names of all extended attributes attached to `path`. Names are
packed into `namebuf` as consecutive NUL-terminated UTF-8 strings with no
padding between them.

**Parameters**
- `path` — Path to the file or directory to query.
- `namebuf` — Buffer to receive the packed name list. Pass `NULL` to query
  the required size without reading anything.
- `size` — Size of `namebuf` in bytes. Pass `0` when `namebuf` is `NULL`.
- `options` — Bitfield of option flags. Use `XATTR_NOFOLLOW` to mirror
  `lstat` semantics (inspect the symlink itself, not its target).

**Return value**
Total byte size of the name list on success (may be `0` if no xattrs exist).
`-1` on error with `errno` set. Notable errors: `ENOTSUP` — filesystem does
not support xattrs; `ERANGE` — `namebuf` too small.

**Notes**
- Use the two-call idiom to avoid `ERANGE`: first call with `NULL`/`0` to get
  the size, then allocate and call again.
  ```c
  ssize_t len = listxattr(path, NULL, 0, XATTR_NOFOLLOW);
  if (len > 0)
  {
      char *buf = malloc(len);
      listxattr(path, buf, len, XATTR_NOFOLLOW);
      // iterate: char *p = buf; while (p < buf + len) { ...; p += strlen(p) + 1; }
      free(buf);
  }
  ```
- For `ft_ls`: a return value `> 0` means the file has xattrs → append `@`
  after the permission string in `-l` output.

---

### `getxattr`

**Prototype**
```c
#include <sys/xattr.h>
ssize_t getxattr(const char *path, const char *name,
                 void *value, size_t size, u_int32_t position, int options);
```

**Overview**
Retrieves the value of a single named extended attribute attached to `path`
and writes up to `size` bytes of it into `value`.

**Parameters**
- `path` — Path to the file whose attribute to read.
- `name` — NUL-terminated UTF-8 name of the attribute (from `listxattr`).
- `value` — Buffer to receive the attribute's data. Pass `NULL` to query the
  data size without reading it.
- `size` — Size of `value` in bytes. Pass `0` when `value` is `NULL`.
- `position` — Byte offset within the attribute data to start reading from.
  Must be `0` for all attributes except `com.apple.ResourceFork`.
- `options` — Same flags as `listxattr`. Use `XATTR_NOFOLLOW`.

**Return value**
Number of bytes written into `value` on success. `0` if the attribute exists
but has no data. `-1` on error; `errno = ENOATTR` means the named attribute
does not exist on this file.

**Notes**
- Same two-call size-query pattern applies: pass `NULL`/`0` first, then
  allocate a buffer and call again.
- The value is **raw bytes**, not necessarily a string.

---

## 5. Time

### `time`

**Prototype**
```c
#include <time.h>
time_t time(time_t *tloc);
```

**Overview**
Returns the current wall-clock time as the number of seconds elapsed since
the Unix epoch (1970-01-01 00:00:00 UTC), not counting leap seconds.

**Parameters**
- `tloc` — If non-NULL, the return value is also written to `*tloc`. Passing
  `NULL` is the common usage.

**Return value**
The current time as a `time_t` on success. `(time_t)-1` on error (rare in
practice).

**Notes**
- Used in `ft_ls` to determine the 6-month threshold that controls timestamp
  format in `-l` output: files modified within the past ~6 months show
  `"Mon DD HH:MM"`; older files show `"Mon DD  YYYY"`.
  ```c
  time_t now = time(NULL);
  if (now - st.st_mtimespec.tv_sec < 6 * 30 * 24 * 3600)
      // use HH:MM format
  else
      // use YYYY format
  ```

---

### `ctime`

**Prototype**
```c
#include <time.h>
char *ctime(const time_t *clock);
```

**Overview**
Converts the `time_t` value pointed to by `clock` into a fixed-format,
26-character human-readable ASCII string representing local time.

**Parameters**
- `clock` — Pointer to a `time_t` value (e.g. `&st.st_mtimespec.tv_sec`).

**Return value**
Pointer to a **static internal buffer** containing the formatted string, or
`NULL` on error. The string is always exactly 26 characters:

```
"Thu Nov 24 18:22:48 1986\n\0"
```

**Notes**
- The static buffer is **overwritten by every call**. Do not store the
  pointer; copy the characters you need immediately.
- All fields are fixed-width, which makes slicing by index reliable:

  | Index range | Content               |
  |-------------|----------------------|
  | `[4..6]`    | Month abbreviation (`Nov`) |
  | `[8..9]`    | Day, space-padded (`24`, ` 4`) |
  | `[11..15]`  | Time (`HH:MM`)       |
  | `[20..23]`  | Year (`1986`)        |

- Because `ctime` respects the locale and the subject requires `LC_ALL=C`,
  month abbreviations will always be the standard English 3-letter forms.

#### `struct timespec` (embedded in `struct stat`)

```c
struct timespec {
    time_t tv_sec;   /* whole seconds since epoch */
    long   tv_nsec;  /* additional nanoseconds (0–999999999) */
};
```

Pass `&st.st_mtimespec.tv_sec` to both `ctime` and the comparison with `time(NULL)`.

---

## 6. Symbolic links

### `readlink`

**Prototype**
```c
#include <unistd.h>
ssize_t readlink(const char *restrict path, char *restrict buf, size_t bufsize);
```

**Overview**
Reads the target string stored inside the symlink at `path` (the path the
link points to) and writes it into `buf`. Does not dereference the link or
validate that the target exists.

**Parameters**
- `path` — Path to the symbolic link itself (not its target).
- `buf` — Caller-allocated buffer to receive the target path string.
- `bufsize` — Size of `buf` in bytes. Pass `sizeof(buf) - 1` to leave room
  for a manually-appended NUL terminator.

**Return value**
Number of bytes written into `buf` on success (the length of the target
string). `-1` on error with `errno` set. Notable errors: `EINVAL` — `path`
is not a symbolic link; `ENAMETOOLONG` — target path exceeds `PATH_MAX`.

**Notes**
- **`readlink` does not NUL-terminate `buf`.** You must append `'\0'` yourself:
  ```c
  ssize_t len = readlink(path, buf, sizeof(buf) - 1);
  if (len != -1)
      buf[len] = '\0';
  ```
- Only call `readlink` when `S_ISLNK(st.st_mode)` is confirmed via `lstat`.
- Used in `-l` mode to print the `-> target` suffix:
  ```
  lrwxrwxrwx  1 user  wheel  7 Apr 24 10:00 mylink -> /tmp/foo
  ```

---

## 7. Memory

### `malloc`

**Prototype**
```c
#include <stdlib.h>
void *malloc(size_t size);
```

**Overview**
Allocates a contiguous block of `size` bytes on the heap and returns a pointer
to it. The contents are **uninitialized** (may contain garbage).

**Parameters**
- `size` — Number of bytes to allocate. Passing `0` is implementation-defined;
  avoid it.

**Return value**
Pointer to the allocated block on success. `NULL` on failure with
`errno = ENOMEM`. Always check the return value.

**Notes**
- Every successful `malloc` must have exactly one matching `free` on every
  code path, including error exits. The project requires zero memory leaks.

---

### `free`

**Prototype**
```c
#include <stdlib.h>
void free(void *ptr);
```

**Overview**
Releases a heap block previously returned by `malloc` (or `calloc`/`realloc`)
back to the allocator.

**Parameters**
- `ptr` — Pointer to the block to free. Passing `NULL` is explicitly defined
  as a no-op — safe to call unconditionally.

**Return value**
None.

**Notes**
- Calling `free` on the same pointer twice is **undefined behaviour** and
  typically corrupts the heap. Set the pointer to `NULL` after freeing to
  prevent accidental double-frees.
- Only pass pointers that were returned directly by `malloc`/`calloc`/
  `realloc`. Never free a pointer into the middle of an allocation or a
  stack-allocated variable.

---

## 8. Error reporting

### `perror`

**Prototype**
```c
#include <stdio.h>
void perror(const char *s);
```

**Overview**
Looks up the human-readable message for the current value of `errno` and
writes it to **stderr**. If `s` is non-NULL and non-empty, it is prepended as
a label followed by `": "`.

Output format: `"s: <error message>\n"` or just `"<error message>\n"`.

**Parameters**
- `s` — Optional prefix string (typically the program name or the path that
  failed). Pass `NULL` or `""` to omit it.

**Return value**
None.

**Notes**
- `perror` reads `errno` at the moment it is called. Do not call any other
  function that might modify `errno` between the failing call and `perror`.
- `ls` error format: `"ls: <path>: <message>"`. Mirror it:
  ```c
  // perror prepends "ft_ls: path" and appends ": <message>\n" automatically
  char prefix[PATH_MAX + 8];
  ft_snprintf(prefix, sizeof(prefix), "ft_ls: %s", path);
  perror(prefix);
  ```

---

### `strerror`

**Prototype**
```c
#include <string.h>
char *strerror(int errnum);
```

**Overview**
Returns a pointer to the human-readable string describing error code `errnum`.
Does not read or modify `errno`.

**Parameters**
- `errnum` — The error code to describe, typically the value of `errno` at the
  point of failure.

**Return value**
Pointer to a **static buffer** containing the message string. For unknown
error codes, returns a string of the form `"Unknown error: N"`. The buffer may
be overwritten by the next call to `strerror`.

**Notes**
- Use `strerror` when you need the message as a string to embed in a larger
  formatted output (e.g. with `ft_printf`). Use `perror` when you only need
  to print directly to stderr.
  ```c
  ft_printf("ft_ls: %s: %s\n", path, strerror(errno));
  ```

---

## 9. Output

### `write`

**Prototype**
```c
#include <unistd.h>
ssize_t write(int fildes, const void *buf, size_t nbyte);
```

**Overview**
Writes up to `nbyte` bytes from `buf` to the file descriptor `fildes`. On
regular files and pipes the write is typically complete in one call; on
sockets or non-blocking descriptors it may write fewer bytes than requested.

**Parameters**
- `fildes` — The file descriptor to write to. For `ft_ls`: `1` (stdout) for
  normal output, `2` (stderr) for error messages.
- `buf` — Pointer to the data to write.
- `nbyte` — Number of bytes to write.

**Return value**
Number of bytes actually written on success (may be less than `nbyte` in edge
cases). `-1` on error with `errno` set.

**Notes**
- `printf` is not allowed in `ft_ls`. Use `ft_printf` from libft instead; it
  calls `write` internally. Reserve direct `write` calls for raw output or
  when building error messages without `perror`.
- Standard file descriptors:

  | fd | Stream |
  |----|--------|
  | 0  | stdin  |
  | 1  | stdout |
  | 2  | stderr |

---

## 10. Exit

### `exit`

**Prototype**
```c
#include <stdlib.h>
void exit(int status);
```

**Overview**
Terminates the process cleanly: calls all `atexit`-registered handlers in
reverse registration order, flushes and closes all open `FILE *` streams, then
hands `status` to the OS.

**Parameters**
- `status` — Exit code returned to the parent process. Only the low 8 bits are
  significant. Conventional values: `0` / `EXIT_SUCCESS` for success,
  `1` / `EXIT_FAILURE` for general errors, `2` for usage errors.

**Return value**
Never returns.

**Notes**
- Mirror `ls` behaviour: on a bad argument or permission error, print the
  error and **continue** processing remaining arguments — do not call `exit`
  immediately. Only `exit(1)` (or `exit(2)`) at the very end if any error
  was encountered.
- `exit` flushes `FILE *` streams (those from `<stdio.h>`), but `ft_printf`
  writes directly via `write(1, ...)` — no buffering to worry about.

---

## Quick reference: what to call for each `-l` column

```
-rwxr-xr-x   2   root   wheel   4096   Apr 24 10:00   filename -> target
^             ^   ^      ^       ^      ^               ^          ^
|             |   |      |       |      |               |          readlink()
|             |   |      |       |      ctime() slice   d_name from readdir()
|             |   |      |       st_size
|             |   |      getgrgid(st_gid)->gr_name
|             |   getpwuid(st_uid)->pw_name
|             st_nlink
build from st_mode bits
```

The `total N` line sums `st_blocks` across all entries in the directory.
`st_blocks` is in 512-byte units; `ls -l` displays 1 KB blocks, so the
printed total equals `sum(st_blocks) / 2`.
