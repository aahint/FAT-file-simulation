#include "vdisk.h"

/* Global variables */
unsigned char *VD = NULL;
unsigned int NBLOCKS, NFREEBLOCKS, RBN;

/* Initialize the virtual disk in shared memory */
void init_disk() {
    unsigned int i, j;
    unsigned char byte_val;
    
    /* Initialize Block 0: total blocks, free blocks, root directory block */
    memcpy(VD + 0, &NBLOCKS, sizeof(unsigned int));
    memcpy(VD + 4, &NFREEBLOCKS, sizeof(unsigned int));
    memcpy(VD + 8, &RBN, sizeof(unsigned int));
    
    /* Initialize Bitmap (Blocks 1-8) */
    /* Mark blocks 0-265 as allocated (not free), rest as free */
    for (i = 0; i < BITMAP_BLOCKS; i++) {
        unsigned char *bitmap_block = VD + ((i + 1) * BLOCK_SIZE);
        memset(bitmap_block, 0xFF, BLOCK_SIZE);  /* Mark all as allocated first */
    }
    
    /* Mark blocks 266 onwards as free */
    for (i = DATA_START_BLOCK; i < TOTAL_BLOCKS; i++) {
        unsigned int block_byte = i / BITS_PER_BYTE;
        unsigned int bit_pos = i % BITS_PER_BYTE;
        unsigned int byte_offset = (i / BITS_PER_BYTE) % BLOCK_SIZE;
        unsigned int block_num = (block_byte / BLOCK_SIZE) + 1;
        unsigned char *byte_ptr = VD + (block_num * BLOCK_SIZE) + byte_offset;
        *byte_ptr &= ~(1 << bit_pos);  /* Clear bit (mark as free) */
    }
    
    /* Initialize FAT (Blocks 9-264) - all pointers set to NULL (0) */
    for (i = 9; i < 9 + FAT_BLOCKS; i++) {
        memset(VD + (i * BLOCK_SIZE), 0, BLOCK_SIZE);
    }
    
    /* Initialize root directory (Block 265) with . and .. entries */
    struct metadata root_dot, root_dotdot;
    
    /* First entry: . (self) */
    root_dot.type = 'd';
    strcpy(root_dot.name, ".");
    root_dot.size = 2;  /* . and .. */
    root_dot.firstblock = ROOT_BLOCK;
    
    /* Second entry: .. (parent - self for root) */
    root_dotdot.type = 'd';
    strcpy(root_dotdot.name, "..");
    root_dotdot.size = 2;  /* Root's size */
    root_dotdot.firstblock = ROOT_BLOCK;  /* Parent is root itself */
    
    memcpy(VD + (ROOT_BLOCK * BLOCK_SIZE), &root_dot, META_SIZE);
    memcpy(VD + (ROOT_BLOCK * BLOCK_SIZE) + META_SIZE, &root_dotdot, META_SIZE);
}

int main(int argc, char *argv[]) {
    int shmid;
    int mode = 0;  /* 0 = create, 1 = remove */
    
    if (argc > 1) {
        if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "remove") == 0) {
            mode = 1;
        }
    }
    
    if (mode == 0) {
        /* Creation mode */
        printf("Creating virtual disk...\n");
        
        /* Create shared memory segment */
        shmid = shmget(SHM_KEY, VD_SIZE, IPC_CREAT | 0666);
        if (shmid < 0) {
            perror("shmget");
            exit(1);
        }
        
        /* Attach to shared memory */
        VD = (unsigned char *)shmat(shmid, NULL, 0);
        if (VD == (unsigned char *)-1) {
            perror("shmat");
            exit(1);
        }
        
        /* Initialize disk parameters */
        NBLOCKS = TOTAL_BLOCKS;
        NFREEBLOCKS = TOTAL_BLOCKS - DATA_START_BLOCK;  /* Blocks 266-65535 are free initially */
        RBN = ROOT_BLOCK;
        
        /* Initialize the disk */
        init_disk();
        
        printf("+++ Virtual Disk Created\n");
        printf("+++ Size: %u blocks (%u MB)\n", NBLOCKS, (NBLOCKS * BLOCK_SIZE) / (1024 * 1024));
        printf("+++ Free blocks: %u\n", NFREEBLOCKS);
        printf("+++ Root block: %u\n", RBN);
        
        /* Detach from shared memory */
        shmdt(VD);
    } else {
        /* Removal mode */
        printf("Removing virtual disk...\n");
        
        shmid = shmget(SHM_KEY, VD_SIZE, 0);
        if (shmid < 0) {
            perror("shmget (remove)");
            exit(1);
        }
        
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            perror("shmctl");
            exit(1);
        }
        
        printf("+++ Virtual Disk Removed\n");
    }
    
    return 0;
}
