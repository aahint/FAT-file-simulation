#include "vdisk.h"
#include <ctype.h>

/* Forward declarations for diskutils functions */
extern unsigned char *D;
extern unsigned int NBLOCKS, NFREEBLOCKS, RBN;

void joindisk();
void leavedisk();
unsigned int getfreeblock();
void freeblock(unsigned int block_num);
int get_metadata(const char *path, const char *cwd, struct metadata *meta);
int find_entry_in_directory(unsigned int dir_block, const char *name, struct metadata *meta);
void print_dir_contents(unsigned int dir_block, int detailed);
int create_directory(const char *path, const char *cwd);
int copy_file(const char *source, const char *dest, const char *cwd, int from_disk);
void print_file(const char *path, const char *cwd);

#include "diskutils.c"

/* ==================== Shell Implementation ==================== */

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64

typedef struct {
    unsigned int cbn;   /* Current block number */
    char cwd[1024];     /* Current working directory */
} shell_state_t;

shell_state_t shell_state;

void initialize_shell() {
    joindisk();
    shell_state.cbn = RBN;
    strcpy(shell_state.cwd, "/");
}

void cleanup_shell() {
    leavedisk();
}

void print_prompt() {
    printf("[foosh] VD:%s> ", shell_state.cwd);
    fflush(stdout);
}

/* Parse and execute cd command */
void cmd_cd(const char *path) {
    char target_path[1024];
    
    if (path == NULL || strlen(path) == 0) {
        /* cd with no argument goes to root */
        shell_state.cbn = RBN;
        strcpy(shell_state.cwd, "/");
        return;
    }
    
    /* Build absolute path */
    if (path[0] == '/') {
        strcpy(target_path, path);
    } else {
        if (strcmp(shell_state.cwd, "/") == 0) {
            snprintf(target_path, sizeof(target_path), "/%s", path);
        } else {
            snprintf(target_path, sizeof(target_path), "%s/%s", shell_state.cwd, path);
        }
    }
    
    /* Remove trailing slashes */
    int len = strlen(target_path);
    while (len > 1 && target_path[len - 1] == '/') {
        target_path[--len] = '\0';
    }
    
    /* Handle special cases */
    if (strcmp(target_path, "/") == 0) {
        shell_state.cbn = RBN;
        strcpy(shell_state.cwd, "/");
        return;
    }
    
    /* Get metadata for target directory */
    struct metadata target_meta;
    if (!get_metadata(target_path, "/", &target_meta) || target_meta.type != 'd') {
        printf("*** Error: unable to change to directory %s\n", path);
        return;
    }
    
    /* Update current state */
    shell_state.cbn = target_meta.firstblock;
    strcpy(shell_state.cwd, target_path);
}

/* Parse and execute md/mkdir command */
void cmd_md(const char *path) {
    if (path == NULL || strlen(path) == 0) {
        fprintf(stderr, "*** Error: mkdir requires a path\n");
        return;
    }
    
    if (!create_directory(path, shell_state.cwd)) {
        return;  /* Error already printed*/
    }
}

/* Parse and execute dir command */
void cmd_dir() {
    print_dir_contents(shell_state.cbn, 0);
}

/* Parse and execute ls command */
void cmd_ls(const char *path) {
    unsigned int list_block;
    
    if (path == NULL || strlen(path) == 0) {
        /* List current directory */
        list_block = shell_state.cbn;
        print_dir_contents(list_block, 1);
        return;
    }
    
    struct metadata target_meta;
    if (!get_metadata(path, shell_state.cwd, &target_meta)) {
        fprintf(stderr, "*** Error: Path not found\n");
        return;
    }
    
    if (target_meta.type == 'f') {
        /* List single file details */
        unsigned int size_width = snprintf(NULL, 0, "%u", target_meta.size);
        unsigned int block_width = snprintf(NULL, 0, "%u", target_meta.firstblock);
        printf("%u f %s %u %u\n", size_width, target_meta.name, target_meta.size, target_meta.firstblock);
    } else {
        /* List directory */
        print_dir_contents(target_meta.firstblock, 1);
    }
}

/* Parse and execute cp/copy command */
void cmd_cp(const char *source, const char *dest) {
    if (source == NULL || dest == NULL) {
        fprintf(stderr, "*** Error: cp requires source and destination\n");
        return;
    }
    
    if (!copy_file(source, dest, shell_state.cwd, 1)) {
        return;  /* Error already printed */
    }
}

/* Parse and execute prn/type command */
void cmd_prn(const char *path) {
    if (path == NULL || strlen(path) == 0) {
        fprintf(stderr, "*** Error: prn requires a file path\n");
        return;
    }
    
    print_file(path, shell_state.cwd);
}

/* Main command processing */
void process_command(const char *cmd_line) {
    char line_copy[MAX_CMD_LEN];
    strcpy(line_copy, cmd_line);
    
    /* Tokenize command */
    char *tokens[MAX_ARGS];
    int token_count = 0;
    char *saveptr;
    char *token = strtok_r(line_copy, " \t", &saveptr);
    
    while (token != NULL && token_count < MAX_ARGS) {
        tokens[token_count++] = token;
        token = strtok_r(NULL, " \t", &saveptr);
    }
    
    if (token_count == 0) return;
    
    /* Convert command to lowercase for comparison */
    char cmd[64];
    strcpy(cmd, tokens[0]);
    for (int i = 0; cmd[i]; i++) cmd[i] = tolower(cmd[i]);
    
    /* Execute command */
    if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        /* Read and print the number of free blocks before exiting */
        unsigned int free_blocks;
        memcpy(&free_blocks, D + 4, sizeof(unsigned int));
        printf("+++ Number of free blocks = %u\n", free_blocks);
        cleanup_shell();
        exit(0);
    } else if (strcmp(cmd, "cd") == 0 || strcmp(cmd, "chdir") == 0) {
        cmd_cd(token_count > 1 ? tokens[1] : NULL);
    } else if (strcmp(cmd, "md") == 0 || strcmp(cmd, "mkdir") == 0) {
        cmd_md(token_count > 1 ? tokens[1] : NULL);
    } else if (strcmp(cmd, "dir") == 0) {
        cmd_dir();
    } else if (strcmp(cmd, "ls") == 0) {
        cmd_ls(token_count > 1 ? tokens[1] : NULL);
    } else if (strcmp(cmd, "cp") == 0 || strcmp(cmd, "copy") == 0) {
        cmd_cp(token_count > 1 ? tokens[1] : NULL, token_count > 2 ? tokens[2] : NULL);
    } else if (strcmp(cmd, "prn") == 0 || strcmp(cmd, "type") == 0) {
        cmd_prn(token_count > 1 ? tokens[1] : NULL);
    } else {
        fprintf(stderr, "*** Error: Unknown command: %s\n", cmd);
    }
}

int main() {
    char cmd_line[MAX_CMD_LEN];
    
    initialize_shell();
    
    while (1) {
        print_prompt();
        
        if (fgets(cmd_line, sizeof(cmd_line), stdin) == NULL) {
            cleanup_shell();
            break;
        }
        
        /* Remove newline */
        int len = strlen(cmd_line);
        if (len > 0 && cmd_line[len - 1] == '\n') {
            cmd_line[len - 1] = '\0';
        }
        
        /* Skip empty lines */
        if (strlen(cmd_line) == 0) continue;
        
        process_command(cmd_line);
    }
    
    return 0;
}
