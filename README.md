# FAT File System Simulator

A working **FAT-style file system** that lives inside a 64 MB shared-memory "virtual disk", with its own shell (`foosh`) for creating folders, moving around and copying files. Written in C for the Operating Systems Laboratory course (CS39002).

Several shells can use the same disk at the same time from different terminals.

## What you can do
| Command | What it does |
|---|---|
| `md` / `mkdir` | Create a directory |
| `cd` / `chdir` | Change directory (absolute and relative paths, `.` and `..`) |
| `dir` | List file names |
| `ls` | List details (type, size, name) |
| `cp` / `copy` | Copy files: real disk to virtual disk, virtual to real, or virtual to virtual |
| `prn` / `type` | Print a file stored on the virtual disk |
| `exit` / `quit` | Leave the shell |

## Quick start
```bash
make                    # builds diskmanager and foosh

./diskmanager           # terminal 1: create the virtual disk
./foosh                 # terminal 2: open the shell

[foosh] VD:/> md mydir
[foosh] VD:/> cd mydir
[foosh] VD:/mydir> cp testfile.txt .
[foosh] VD:/mydir> ls
[foosh] VD:/mydir> prn testfile.txt
[foosh] VD:/mydir> exit

./diskmanager -r        # remove the virtual disk when finished
```

## How the disk is laid out
The disk has 65,536 blocks of 1 KB each.

| Blocks | Contents |
|---|---|
| 0 | Metadata: total blocks, free blocks, root block |
| 1-8 | Bitmap: one bit per block, marks which blocks are free |
| 9-264 | FAT: 32-bit pointers that chain a file's blocks together |
| 265 | Root directory (contains `.` and `..`) |
| 266-65535 | File data and sub-directories |

## Design decisions
- **Bitmap for free blocks:** bit operations make allocation and freeing fast.
- **FAT chaining:** each block's FAT entry points to the next block of the file, so large files can be stored in scattered blocks.
- **Random block allocation:** blocks are picked at random to mimic non-contiguous storage on a real disk.
- **Directory entries:** a 32-byte record (type, name up to 22 characters, size, first block), so 32 entries fit in one block. Directories are simple unsorted lists.
- **Overwriting:** copying onto an existing file frees its old blocks first.
- **No locking:** as the assignment specified, there are no mutexes, so concurrent shells are expected to be used manually.

## Code map
| File | Purpose |
|---|---|
| `vdisk.h` | Constants, data structures and disk layout |
| `diskmanager.c` | Creates and removes the shared-memory disk |
| `diskutils.c` | Library: block allocation, FAT chaining, path resolution, copy, list, print |
| `foosh.c` | The shell and its commands |
| `makefile` | Targets: `all`, `diskmanager`, `foosh`, `clean` |

## What I learned
How FAT works from the inside, shared memory between processes, bit-level bookkeeping, and path resolution.
