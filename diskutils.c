#include "vdisk.h"
#include <time.h>

/* Global variables - these will be initialized by joindisk() */
unsigned char *D = NULL;
unsigned int NBLOCKS, NFREEBLOCKS, RBN;

/* ==================== Basic Disk Functions ==================== */

void joindisk() {
    int shmid;
    
    shmid = shmget(SHM_KEY, VD_SIZE, 0);
    if (shmid < 0) {
        perror("shmget (joindisk)");
        exit(1);
    }
    
    D = (unsigned char *)shmat(shmid, NULL, 0);
    if (D == (unsigned char *)-1) {
        perror("shmat (joindisk)");
        exit(1);
    }
    
    /* Read initialization parameters from Block 0 */
    memcpy(&NBLOCKS, D + 0, sizeof(unsigned int));
    memcpy(&NFREEBLOCKS, D + 4, sizeof(unsigned int));
    memcpy(&RBN, D + 8, sizeof(unsigned int));
    
    printf("+++ Number of blocks = %u\n", NBLOCKS);
    printf("+++ Number of free blocks = %u\n", NFREEBLOCKS);
    printf("+++ First block of the root directory = %u\n", RBN);
}

void leavedisk() {
    if (D != NULL) {
        /* Update free block count before leaving */
        memcpy(D + 4, &NFREEBLOCKS, sizeof(unsigned int));
        shmdt(D);
    }
}

/* Check if a block is free using the bitmap */
int is_block_free(unsigned int block_num) {
    if (block_num >= TOTAL_BLOCKS) return 0;
    
    unsigned int byte_pos = block_num / BITS_PER_BYTE;
    unsigned int bit_pos = block_num % BITS_PER_BYTE;
    unsigned int block_num_in_bitmap = (byte_pos / BLOCK_SIZE) + 1;
    unsigned int offset_in_block = byte_pos % BLOCK_SIZE;
    
    unsigned char *bitmap_byte = D + (block_num_in_bitmap * BLOCK_SIZE) + offset_in_block;
    return (*bitmap_byte & (1 << bit_pos)) == 0;
}

/* Set a block as allocated in the bitmap */
void set_block_allocated(unsigned int block_num) {
    if (block_num >= TOTAL_BLOCKS) return;
    
    unsigned int byte_pos = block_num / BITS_PER_BYTE;
    unsigned int bit_pos = block_num % BITS_PER_BYTE;
    unsigned int block_num_in_bitmap = (byte_pos / BLOCK_SIZE) + 1;
    unsigned int offset_in_block = byte_pos % BLOCK_SIZE;
    
    unsigned char *bitmap_byte = D + (block_num_in_bitmap * BLOCK_SIZE) + offset_in_block;
    *bitmap_byte |= (1 << bit_pos);
}

/* Set a block as free in the bitmap */
void set_block_free(unsigned int block_num) {
    if (block_num >= TOTAL_BLOCKS) return;
    
    unsigned int byte_pos = block_num / BITS_PER_BYTE;
    unsigned int bit_pos = block_num % BITS_PER_BYTE;
    unsigned int block_num_in_bitmap = (byte_pos / BLOCK_SIZE) + 1;
    unsigned int offset_in_block = byte_pos % BLOCK_SIZE;
    
    unsigned char *bitmap_byte = D + (block_num_in_bitmap * BLOCK_SIZE) + offset_in_block;
    *bitmap_byte &= ~(1 << bit_pos);
}

unsigned int getfreeblock() {
    unsigned int block_num;
    srand(time(NULL) ^ getpid());
    
    do {
        block_num = DATA_START_BLOCK + (rand() % (TOTAL_BLOCKS - DATA_START_BLOCK));
    } while (!is_block_free(block_num));
    
    set_block_allocated(block_num);
    NFREEBLOCKS--;
    memcpy(D + 4, &NFREEBLOCKS, sizeof(unsigned int));
    
    return block_num;
}

void freeblock(unsigned int block_num) {
    if (block_num < DATA_START_BLOCK || block_num >= TOTAL_BLOCKS) return;
    
    set_block_free(block_num);
    NFREEBLOCKS++;
    memcpy(D + 4, &NFREEBLOCKS, sizeof(unsigned int));
}

/* Get next block from FAT */
unsigned int get_next_block(unsigned int block_num) {
    if (block_num >= TOTAL_BLOCKS) return NULL_BLOCK;
    
    unsigned int fat_block_num = 9 + (block_num / PTRS_PER_FAT_BLOCK);
    unsigned int offset_in_block = (block_num % PTRS_PER_FAT_BLOCK) * sizeof(unsigned int);
    unsigned int next_block;
    
    memcpy(&next_block, D + (fat_block_num * BLOCK_SIZE) + offset_in_block, sizeof(unsigned int));
    return next_block;
}

/* Set next block in FAT */
void set_next_block(unsigned int block_num, unsigned int next_block) {
    if (block_num >= TOTAL_BLOCKS) return;
    
    unsigned int fat_block_num = 9 + (block_num / PTRS_PER_FAT_BLOCK);
    unsigned int offset_in_block = (block_num % PTRS_PER_FAT_BLOCK) * sizeof(unsigned int);
    
    memcpy(D + (fat_block_num * BLOCK_SIZE) + offset_in_block, &next_block, sizeof(unsigned int));
}

/* ==================== Path and Metadata Functions ==================== */

/* Resolve a path to a metadata entry */
int get_metadata(const char *path, const char *cwd, struct metadata *meta) {
    char full_path[1024];
    char *path_copy;
    char *token, *saveptr;
    struct metadata entry;
    unsigned int current_block;
    int found;
    
    /* Handle empty path */
    if (path == NULL || strlen(path) == 0) {
        return 0;
    }
    
    /* Construct full path */
    if (path[0] == '/') {
        strcpy(full_path, path);
    } else {
        if (strcmp(cwd, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", path);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", cwd, path);
        }
    }
    
    /* Remove trailing slashes (except for root) */
    int len = strlen(full_path);
    while (len > 1 && full_path[len - 1] == '/') {
        full_path[--len] = '\0';
    }
    
    /* Handle root directory */
    if (strcmp(full_path, "/") == 0) {
        memcpy(meta, D + (RBN * BLOCK_SIZE), META_SIZE);
        return 1;
    }
    
    /* Parse path components and traverse directory tree */
    path_copy = strdup(full_path);
    current_block = RBN;
    token = strtok_r(path_copy + 1, "/", &saveptr);
    
    while (token != NULL) {
        unsigned int search_block = current_block;
        found = 0;
        
        /* Search for token in current directory */
        while (1) {
            for (unsigned int i = 0; i < ENTRIES_PER_BLOCK; i++) {
                memcpy(&entry, D + (search_block * BLOCK_SIZE) + (i * META_SIZE), META_SIZE);
                if (entry.name[0] == '\0') break;
                
                if (strcmp(entry.name, token) == 0) {
                    found = 1;
                    break;
                }
            }
            
            if (found) break;
            
            unsigned int next = get_next_block(search_block);
            if (next == NULL_BLOCK) break;
            search_block = next;
        }
        
        if (!found) {
            free(path_copy);
            return 0;
        }
        
        *meta = entry;
        current_block = entry.firstblock;
        token = strtok_r(NULL, "/", &saveptr);
    }
    
    free(path_copy);
    return 1;
}

/* Get metadata of entry in a directory */
int find_entry_in_directory(unsigned int dir_block, const char *name, struct metadata *meta) {
    struct metadata entry;
    unsigned int current_block = dir_block;
    
    while (1) {
        for (unsigned int i = 0; i < ENTRIES_PER_BLOCK; i++) {
            memcpy(&entry, D + (current_block * BLOCK_SIZE) + (i * META_SIZE), META_SIZE);
            if (entry.name[0] == '\0') return 0;
            
            if (strcmp(entry.name, name) == 0) {
                *meta = entry;
                return 1;
            }
        }
        
        unsigned int next = get_next_block(current_block);
        if (next == NULL_BLOCK) return 0;
        current_block = next;
    }
}

/* ==================== Directory Listing ==================== */

void print_dir_contents(unsigned int dir_block, int detailed) {
    struct metadata entry;
    unsigned int current_block = dir_block;
    unsigned int entry_count = 0;
    unsigned int total_entries = 0;
    unsigned int parent_block = 0;
    unsigned int parent_entry_count = 0;
    
    /* First pass: count total entries and get parent block */
    current_block = dir_block;
    while (1) {
        for (unsigned int i = 0; i < ENTRIES_PER_BLOCK; i++) {
            memcpy(&entry, D + (current_block * BLOCK_SIZE) + (i * META_SIZE), META_SIZE);
            if (entry.name[0] == '\0') break;
            
            /* Capture parent block number from .. entry */
            if (strcmp(entry.name, "..") == 0) {
                parent_block = entry.firstblock;
            }
            
            total_entries++;
        }
        
        unsigned int next = get_next_block(current_block);
        if (next == NULL_BLOCK) break;
        current_block = next;
    }
    
    /* Count entries in parent directory (if exists) */
    if (parent_block != 0) {
        unsigned int parent_current_block = parent_block;
        while (1) {
            for (unsigned int i = 0; i < ENTRIES_PER_BLOCK; i++) {
                struct metadata parent_entry;
                memcpy(&parent_entry, D + (parent_current_block * BLOCK_SIZE) + (i * META_SIZE), META_SIZE);
                if (parent_entry.name[0] == '\0') break;
                parent_entry_count++;
            }
            
            unsigned int next = get_next_block(parent_current_block);
            if (next == NULL_BLOCK) break;
            parent_current_block = next;
        }
    }
    
    if (detailed) {
        printf("Total %u entries\n", total_entries);
        printf("-----------------------------------------------------------------------\n");
        printf("TYPE NAME SIZE FIRST BLOCK\n");
        printf("-----------------------------------------------------------------------\n");
    }
    
    /* Second pass: print entries */
    current_block = dir_block;
    while (1) {
        for (unsigned int i = 0; i < ENTRIES_PER_BLOCK; i++) {
            memcpy(&entry, D + (current_block * BLOCK_SIZE) + (i * META_SIZE), META_SIZE);
            if (entry.name[0] == '\0') break;
            
            /* Use dynamic size for . and .. entries */
            unsigned int display_size = entry.size;
            if (strcmp(entry.name, ".") == 0) {
                display_size = total_entries;
            } else if (strcmp(entry.name, "..") == 0) {
                display_size = parent_entry_count;
            }
            
            if (detailed) {
                printf(" %c %-22s %u %u\n", entry.type, entry.name, display_size, entry.firstblock);
            } else {
                printf(" %s", entry.name);
            }
            entry_count++;
        }
        
        unsigned int next = get_next_block(current_block);
        if (next == NULL_BLOCK) break;
        current_block = next;
    }
    
    if (!detailed) printf("\n");
    if (detailed) printf("-----------------------------------------------------------------------\n");
}

/* ==================== Directory Creation ==================== */

int create_directory(const char *path, const char *cwd) {
    char full_path[1024];
    
    /* Construct full path */
    if (path[0] == '/') {
        strcpy(full_path, path);
    } else {
        if (strcmp(cwd, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", path);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", cwd, path);
        }
    }
    
    /* Remove trailing slashes */
    int len = strlen(full_path);
    while (len > 1 && full_path[len - 1] == '/') {
        full_path[--len] = '\0';
    }
    
    /* Find parent directory and new directory name */
    char *last_slash = strrchr(full_path, '/');
    char parent_path[1024];
    char dir_name[23];
    
    if (last_slash == full_path) {
        strcpy(parent_path, "/");
        strcpy(dir_name, last_slash + 1);
    } else {
        strncpy(parent_path, full_path, last_slash - full_path);
        parent_path[last_slash - full_path] = '\0';
        strcpy(dir_name, last_slash + 1);
    }
    
    /* Get parent directory metadata */
    struct metadata parent_meta;
    if (strcmp(parent_path, "/") == 0) {
        memcpy(&parent_meta, D + (RBN * BLOCK_SIZE), META_SIZE);
    } else {
        if (!get_metadata(parent_path, "/", &parent_meta) || parent_meta.type != 'd') {
            fprintf(stderr, "*** Error: parent directory not found\n");
            return 0;
        }
    }
    
    /* Check if name already exists */
    struct metadata dummy;
    if (find_entry_in_directory(parent_meta.firstblock, dir_name, &dummy)) {
        fprintf(stderr, "*** Error: directory already exists\n");
        return 0;
    }
    
    /* Get a free block for new directory */
    unsigned int new_dir_block = getfreeblock();
    
    /* Create . and .. entries in new directory */
    struct metadata dot, dotdot;
    dot.type = 'd';
    strcpy(dot.name, ".");
    dot.size = 2;
    dot.firstblock = new_dir_block;
    
    dotdot.type = 'd';
    strcpy(dotdot.name, "..");
    dotdot.size = parent_meta.size;  /* Parent's entry count */
    dotdot.firstblock = parent_meta.firstblock;
    
    memcpy(D + (new_dir_block * BLOCK_SIZE), &dot, META_SIZE);
    memcpy(D + (new_dir_block * BLOCK_SIZE) + META_SIZE, &dotdot, META_SIZE);
    
    /* Add entry to parent directory */
    struct metadata new_entry;
    new_entry.type = 'd';
    strcpy(new_entry.name, dir_name);
    new_entry.size = 2;
    new_entry.firstblock = new_dir_block;
    
    /* Find where to insert the new entry in parent directory */
    unsigned int parent_block = parent_meta.firstblock;
    struct metadata parent_dot;
    memcpy(&parent_dot, D + (parent_block * BLOCK_SIZE), META_SIZE);
    parent_dot.size++;  /* Increment directory entry count */
    memcpy(D + (parent_block * BLOCK_SIZE), &parent_dot, META_SIZE);
    
    /* Find free slot in parent directory */
    while (1) {
        int found_slot = 0;
        for (unsigned int i = 0; i < ENTRIES_PER_BLOCK; i++) {
            struct metadata entry;
            memcpy(&entry, D + (parent_block * BLOCK_SIZE) + (i * META_SIZE), META_SIZE);
            if (entry.name[0] == '\0') {
                memcpy(D + (parent_block * BLOCK_SIZE) + (i * META_SIZE), &new_entry, META_SIZE);
                found_slot = 1;
                break;
            }
        }
        
        if (found_slot) break;
        
        /* Need another block for parent directory */
        unsigned int next = get_next_block(parent_block);
        if (next == NULL_BLOCK) {
            unsigned int new_block = getfreeblock();
            set_next_block(parent_block, new_block);
            parent_block = new_block;
        } else {
            parent_block = next;
        }
    }
    
    return 1;
}

/* ==================== File Copying ==================== */

int copy_file_to_vd(const char *hd_file, const char *vd_dest, const char *cwd);
int copy_file_to_hd(const char *vd_file, const char *hd_dest, const char *cwd);
int copy_file_in_vd(const char *source, const char *dest, const char *cwd);

int copy_file_to_vd(const char *hd_file, const char *vd_dest, const char *cwd) {
    FILE *src_file = fopen(hd_file, "rb");
    if (!src_file) {
        fprintf(stderr, "*** Error: Unable to read input file %s\n", hd_file);
        return 0;
    }
    
    fseek(src_file, 0, SEEK_END);
    unsigned int file_size = ftell(src_file);
    fseek(src_file, 0, SEEK_SET);
    
    /* Determine full destination path and name */
    char full_dest[1024];
    if (vd_dest[0] == '/') {
        strcpy(full_dest, vd_dest);
    } else {
        if (strcmp(cwd, "/") == 0) {
            snprintf(full_dest, 1024, "/%s", vd_dest);
        } else {
            snprintf(full_dest, 1024, "%s/%s", cwd, vd_dest);
        }
    }
    
    int len = strlen(full_dest);
    while (len > 1 && full_dest[len - 1] == '/') {
        full_dest[--len] = '\0';
    }
    
    /* Check if destination is a directory */
    struct metadata dest_meta;
    int is_dir = 0;
    if (get_metadata(full_dest, "/", &dest_meta) && dest_meta.type == 'd') {
        is_dir = 1;
    }
    
    char file_name[23];
    char parent_path[1024];
    
    if (is_dir) {
        /* Extract source file name and use it */
        char *last_slash = strrchr(hd_file, '/');
        if (last_slash) {
            strcpy(file_name, last_slash + 1);
        } else {
            strcpy(file_name, hd_file);
        }
        strcpy(parent_path, full_dest);
    } else {
        /* Split destination into parent and file name */
        char *last_slash = strrchr(full_dest, '/');
        if (last_slash == full_dest) {
            strcpy(parent_path, "/");
        } else {
            strncpy(parent_path, full_dest, last_slash - full_dest);
            parent_path[last_slash - full_dest] = '\0';
        }
        strcpy(file_name, last_slash ? last_slash + 1 : full_dest);
    }
    
    /* Get parent directory */
    struct metadata parent_meta;
    if (strcmp(parent_path, "/") == 0) {
        memcpy(&parent_meta, D + (RBN * BLOCK_SIZE), META_SIZE);
    } else {
        if (!get_metadata(parent_path, "/", &parent_meta) || parent_meta.type != 'd') {
            fprintf(stderr, "*** Error: Parent directory not found\n");
            fclose(src_file);
            return 0;
        }
    }
    
    /* Free old file if exists */
    struct metadata old_entry;
    if (find_entry_in_directory(parent_meta.firstblock, file_name, &old_entry)) {
        unsigned int block = old_entry.firstblock;
        while (block != NULL_BLOCK) {
            unsigned int next = get_next_block(block);
            freeblock(block);
            block = next;
        }
    }
    
    /* Allocate and write file blocks */
    unsigned char buffer[BLOCK_SIZE];
    unsigned int first_block = getfreeblock();
    unsigned int current_block = first_block;
    unsigned int bytes_read = 0;
    
    while ((bytes_read = fread(buffer, 1, BLOCK_SIZE, src_file)) > 0) {
        memcpy(D + (current_block * BLOCK_SIZE), buffer, bytes_read);
        
        if (bytes_read < BLOCK_SIZE) break;
        
        unsigned int next_block = getfreeblock();
        set_next_block(current_block, next_block);
        current_block = next_block;
    }
    
    fclose(src_file);
    
    /* Add/update entry in parent directory */
    struct metadata file_entry;
    file_entry.type = 'f';
    strcpy(file_entry.name, file_name);
    file_entry.size = file_size;
    file_entry.firstblock = first_block;
    
    if (!find_entry_in_directory(parent_meta.firstblock, file_name, &old_entry)) {
        struct metadata parent_dot;
        memcpy(&parent_dot, D + (parent_meta.firstblock * BLOCK_SIZE), META_SIZE);
        parent_dot.size++;
        memcpy(D + (parent_meta.firstblock * BLOCK_SIZE), &parent_dot, META_SIZE);
    }
    
    unsigned int parent_block = parent_meta.firstblock;
    while (1) {
        int found = 0;
        for (unsigned int i = 0; i < ENTRIES_PER_BLOCK; i++) {
            struct metadata entry;
            memcpy(&entry, D + (parent_block * BLOCK_SIZE) + (i * META_SIZE), META_SIZE);
            if (entry.name[0] == '\0' || strcmp(entry.name, file_name) == 0) {
                memcpy(D + (parent_block * BLOCK_SIZE) + (i * META_SIZE), &file_entry, META_SIZE);
                found = 1;
                break;
            }
        }
        
        if (found) break;
        
        unsigned int next = get_next_block(parent_block);
        if (next == NULL_BLOCK) {
            unsigned int new_block = getfreeblock();
            set_next_block(parent_block, new_block);
            parent_block = new_block;
        } else {
            parent_block = next;
        }
    }
    
    return 1;
}

int copy_file_to_hd(const char *vd_file, const char *hd_file, const char *cwd) {
    struct metadata src_meta;
    if (!get_metadata(vd_file, cwd, &src_meta) || src_meta.type != 'f') {
        fprintf(stderr, "*** Error: Unable to read input file\n");
        return 0;
    }
    
    FILE *dst_file = fopen(hd_file, "wb");
    if (!dst_file) {
        fprintf(stderr, "*** Error: Cannot create output file\n");
        return 0;
    }
    
    unsigned int block = src_meta.firstblock;
    unsigned int remaining = src_meta.size;
    
    while (block != NULL_BLOCK && remaining > 0) {
        unsigned int to_copy = (remaining > BLOCK_SIZE) ? BLOCK_SIZE : remaining;
        fwrite(D + (block * BLOCK_SIZE), 1, to_copy, dst_file);
        remaining -= to_copy;
        block = get_next_block(block);
    }
    
    fclose(dst_file);
    return 1;
}

int copy_file_in_vd(const char *source, const char *dest, const char *cwd) {
    struct metadata src_meta;
    if (!get_metadata(source, cwd, &src_meta) || src_meta.type != 'f') {
        fprintf(stderr, "*** Error: Source file not found\n");
        return 0;
    }
    
    /* For now, just copy within VD (same logic as to_vd but source is in VD) */
    /* This is complex and would need similar logic */
    fprintf(stderr, "*** Error: VD to VD copy not fully implemented\n");
    return 0;
}

int copy_file(const char *source, const char *dest, const char *cwd, int from_disk) {
    /* Check for backticks to determine source/dest location */
    int src_is_hd = (source != NULL && source[0] == '`');
    int dst_is_hd = (dest != NULL && dest[0] == '`');
    
    if (src_is_hd && dst_is_hd) {
        fprintf(stderr, "*** Error: Cannot copy between two HD files\n");
        return 0;
    }
    
    if (src_is_hd) {
        /* Copy from HD to VD */
        return copy_file_to_vd(source + 1, dest, cwd);
    } else if (dst_is_hd) {
        /* Copy from VD to HD */
        return copy_file_to_hd(source, dest + 1, cwd);
    } else {
        /* Copy within VD */
        return copy_file_in_vd(source, dest, cwd);
    }
}

/* ==================== File Printing ==================== */

void print_file(const char *path, const char *cwd) {
    struct metadata file_meta;
    if (!get_metadata(path, cwd, &file_meta) || file_meta.type != 'f') {
        fprintf(stderr, "*** Error: File not found\n");
        return;
    }
    
    unsigned int block = file_meta.firstblock;
    unsigned int remaining = file_meta.size;
    
    while (block != NULL_BLOCK && remaining > 0) {
        unsigned int to_print = (remaining > BLOCK_SIZE) ? BLOCK_SIZE : remaining;
        fwrite(D + (block * BLOCK_SIZE), 1, to_print, stdout);
        remaining -= to_print;
        block = get_next_block(block);
    }
}
