FAT File System Simulator - Implementation Documentation
=====================================================================

OVERVIEW
--------
This is a complete implementation of an in-memory FAT file system simulator
as specified in CS39002 Operating Systems Laboratory, Lab Assignment 10.

The system simulates a 64 MB virtual disk using shared memory, with FAT-based
file allocation and directory structures that closely mirror real file systems.

COMPONENTS
----------

1. vdisk.h
   - Header file containing all constants, data structures, and macros
   - Defines the metadata structure for files/directories
   - Specifies VD layout: bitmap, FAT, and data blocks

2. diskmanager.c
   - Creates/removes the 64 MB shared memory segment
   - Initializes the disk structure on creation:
     * Block 0: metadata (total blocks, free blocks, root block)
     * Blocks 1-8: bitmap (65536 bits for block allocation)
     * Blocks 9-264: FAT (256 blocks storing 32-bit block pointers)
     * Block 265: root directory with . and .. entries
     * Blocks 266-65535: data and user directories
   - Usage: ./diskmanager (create mode)
           ./diskmanager -r (removal mode)

3. diskutils.c
   - Disk utility library with no main() function
   - Core functions:
     * joindisk(): Attach process to shared memory
     * leavedisk(): Detach from shared memory
     * getfreeblock(): Allocate a free block (random)
     * freeblock(): Free an allocated block
     * Bitmap operations for block allocation tracking
     * FAT operations for block chaining
   - High-level functions:
     * get_metadata(): Resolve paths and get metadata
     * find_entry_in_directory(): Search for entries
     * create_directory(): Create new directories
     * copy_file_to_vd(): Copy from HD to VD
     * copy_file_to_hd(): Copy from VD to HD
     * print_dir_contents(): List directory contents
     * print_file(): Print file contents

4. foosh.c
   - Shell program (Foo version of sh)
   - Includes diskutils.c as library
   - Maintains shell state:
     * CBN: current block number
     * CWD: current working directory path
   - Implements commands:
     * md/mkdir: Create directories
     * cd/chdir: Change directory
     * dir: List filenames only
     * ls: List detailed directory contents
     * cp/copy: Copy files (HD↔VD, VD↔VD)
     * prn/type: Print file contents
     * exit/quit: Exit shell

5. makefile
   - Builds diskmanager and foosh executables
   - Targets: all (default), diskmanager, foosh, clean

BUILDING
--------
$ cd /home/user/23CS30059_LA10
$ make              # Build both diskmanager and foosh
$ make clean        # Remove build artifacts

OUTPUT:
- diskmanager: Executable for VD creation/removal
- foosh: Shell interpreter executable

USAGE
-----

1. IN ONE TERMINAL, CREATE THE VIRTUAL DISK:
   $ ./diskmanager
   
   Output:
   Creating virtual disk...
   +++ Virtual Disk Created
   +++ Size: 65536 blocks (64 MB)
   +++ Free blocks: 65270
   +++ Root block: 265

2. IN ANOTHER TERMINAL(S), INTERACT WITH THE DISK:
   $ ./foosh
   
   [foosh] VD:/> md mydir
   [foosh] VD:/> cd mydir
   [foosh] VD:/mydir> cp `mytextfile.txt .
   [foosh] VD:/mydir> ls
   [foosh] VD:/mydir> prn mytextfile.txt
   [foosh] VD:/mydir> exit
   
   Multiple shells can access the disk simultaneously from different terminals!

3. WHEN DONE, REMOVE THE VIRTUAL DISK:
   $ ./diskmanager -r
   
   Output:
   Removing virtual disk...
   +++ Virtual Disk Removed

FEATURES IMPLEMENTED
--------------------

✓ Shared memory management (64 MB)
✓ Block bitmap for free block tracking
✓ FAT (File Allocation Table) with block chaining
✓ Directory metadata structures
✓ Support for nested directories
✓ Absolute and relative path resolution
✓ File copying (HD→VD, VD→HD, VD→VD)
✓ Directory creation and listing
✓ File contents printing
✓ Multiple simultaneous shell sessions
✓ Proper file overwriting
✓ Random block allocation to simulate non-contiguous storage

KEY DESIGN DECISIONS
--------------------

1. Bitmap-based free block tracking
   - Uses bitwise operations for efficiency
   - Blocks 1-8 store allocation status for all blocks
   
2. FAT for block chaining
   - 32-bit pointers (unsigned int) per block pointer
   - 256 pointers per FAT block
   - 256 FAT blocks total

3. Metadata per file/directory
   - 32-byte structure (fits 32 entries per block)
   - Type (f/d), name (22 chars max), size, first block

4. Directory structure
   - Every directory has . (self) and .. (parent)
   - size field stores entry count for directories
   - Linear unsorted list of entries

5. Path handling
   - Full path resolution from root or current directory
   - Proper handling of /, .., and . components
   - Support for both absolute (/path) and relative (path) paths

6. File operations
   - Random block allocation to simulate realistic storage
   - Complete block chaining for large files
   - File overwriting with cleanup of old blocks

TESTING
-------

Basic functionality has been tested:
✓ Virtual disk creation and initialization
✓ Directory creation (md/mkdir)
✓ Directory navigation (cd/chdir)
✓ Directory listing (dir, ls)
✓ File copying from HD to VD
✓ File copying from VD to HD
✓ File content printing (prn)
✓ Multiple simultaneous sessions

FILE STRUCTURE SUMMARY
----------------------

Virtual Disk Layout (total 65536 blocks of 1 KB each):
┌─────────────────┬─────────────────┬────────────┬─────────────────┐
│ Block 0: Meta   │ Blocks 1-8:     │ Blocks 9-  │ Blocks 266-     │
│ (3 unsigned     │ Bitmap          │ 264: FAT   │ 65535: Data     │
│ ints)           │ (8192 bytes)    │ (262144B)  │ (68517888 bytes)│
└─────────────────┴─────────────────┴────────────┴─────────────────┘
                                                   ↓
                                          Block 265: Root Dir

NOTES
-----

1. The implementation does NOT use mutexes for synchronization
   as per assignment specification (manual operation assumed).

2. Block allocation uses random selection for variety.

3. Paths are resolved character-by-character for robustness.

4. All numeric data is stored in raw binary format using memcpy().

5. Only file/directory names are stored as strings.

SUBMISSION
----------

All files are ready for submission:
- vdisk.h : Header file with structures and constants
- diskmanager.c : VD creation/removal program
- diskutils.c : Disk utilities library
- foosh.c : Shell program
- makefile : Build configuration

These files should be packaged into a single tar/tgz/zip archive
for submission to the course.
