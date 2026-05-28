/*
 * filesystem.c — 用户态文件系统模拟器
 *
 * 在磁盘镜像 disk.img 上模拟:
 *   - 超级块 + i 节点表 + 数据块
 *   - 位图管理空闲块/空闲 i 节点
 *   - 支持文件/目录的创建、读写、删除
 *   - Shell 风格交互界面
 *
 * 磁盘布局 (每块 512B, 共 1024 块 ≈ 512KB):
 *   Block 0      : 超级块
 *   Block 1      : inode 位图 (64 inode)
 *   Block 2      : 数据块位图 (1024 blocks)
 *   Block 3-6    : inode 表 (64 * 32B = 2048B = 4 blocks)
 *   Block 7-1023 : 数据块
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ============ 常量 ============ */

#define BLOCK_SIZE      512
#define NUM_BLOCKS      1024
#define NUM_INODES      64
#define MAX_FILENAME    28
#define DIRECT_PTRS     8
#define INODES_PER_BLOCK (BLOCK_SIZE / 32)   /* 512/32 = 16 */
#define INODE_BITMAP_BLK 1
#define DATA_BITMAP_BLK  2
#define INODE_START_BLK  3
#define INODE_BLOCKS     ((NUM_INODES + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK)
#define DATA_START_BLK   (INODE_START_BLK + INODE_BLOCKS)
#define ROOT_INODE       0

#define TYPE_FREE  0
#define TYPE_FILE  1
#define TYPE_DIR   2

/* ============ 磁盘结构 ============ */

typedef struct {
    int32_t magic;           /* 0x4F534CAB */
    int32_t num_blocks;
    int32_t num_inodes;
    int32_t free_blocks;
    int32_t free_inodes;
    int32_t inode_start;
    int32_t data_start;
    char    padding[BLOCK_SIZE - 7 * 4];
} Superblock;

typedef struct {
    int16_t type;                   /* 0=free, 1=file, 2=dir */
    int16_t links;                  /* 硬链接数 */
    int32_t size;                   /* 文件大小 (字节) */
    int32_t direct[DIRECT_PTRS];    /* 直接块指针 */
    int32_t indirect;               /* 一级间接块指针 */
    int32_t ctime;                  /* 创建/修改时间 */
} Inode;  /* 共 4+4+4+32+4+4 = 52B -> padding 到 64B 对齐 */

/* 确保 Inode 大小可预测 */
_Static_assert(sizeof(Inode) <= 64, "Inode too large");

typedef struct {
    char    name[MAX_FILENAME];
    int32_t inode_num;
} DirEntry;  /* 32B, 每块 16 个条目 */

static void bitmap_set(uint8_t *bitmap, int idx, bool val) {
    if (val) bitmap[idx / 8] |=  (1 << (idx % 8));
    else     bitmap[idx / 8] &= ~(1 << (idx % 8));
}

static bool bitmap_get(uint8_t *bitmap, int idx) {
    return bitmap[idx / 8] & (1 << (idx % 8));
}

/* ============ 全局状态 ============ */

static Superblock  g_sb;
static uint8_t     g_inode_bmp[BLOCK_SIZE];
static uint8_t     g_block_bmp[BLOCK_SIZE];
static FILE        *g_disk = NULL;

/* ============ 低级 I/O ============ */

static void disk_read(int blkno, void *buf) {
    fseek(g_disk, blkno * BLOCK_SIZE, SEEK_SET);
    fread(buf, 1, BLOCK_SIZE, g_disk);
}

static void disk_write(int blkno, const void *buf) {
    fseek(g_disk, blkno * BLOCK_SIZE, SEEK_SET);
    fwrite(buf, 1, BLOCK_SIZE, g_disk);
    fflush(g_disk);
}

static void disk_zero(int blkno) {
    char zero[BLOCK_SIZE] = {0};
    disk_write(blkno, zero);
}

/* ============ 分配/释放 ============ */

static int alloc_inode(void) {
    for (int i = 0; i < NUM_INODES; i++) {
        if (!bitmap_get(g_inode_bmp, i)) {
            bitmap_set(g_inode_bmp, i, true);
            g_sb.free_inodes--;
            disk_write(INODE_BITMAP_BLK, g_inode_bmp);
            return i;
        }
    }
    return -1;
}

static void free_inode(int ino) {
    bitmap_set(g_inode_bmp, ino, false);
    g_sb.free_inodes++;
    disk_write(INODE_BITMAP_BLK, g_inode_bmp);
}

static int alloc_block(void) {
    for (int i = DATA_START_BLK; i < NUM_BLOCKS; i++) {
        if (!bitmap_get(g_block_bmp, i)) {
            bitmap_set(g_block_bmp, i, true);
            g_sb.free_blocks--;
            disk_write(DATA_BITMAP_BLK, g_block_bmp);
            disk_zero(i);
            return i;
        }
    }
    return -1;
}

static void free_block(int blkno) {
    if (blkno < DATA_START_BLK || blkno >= NUM_BLOCKS) return;
    bitmap_set(g_block_bmp, blkno, false);
    g_sb.free_blocks++;
    disk_write(DATA_BITMAP_BLK, g_block_bmp);
}

/* 读 inode */
static void read_inode(int ino, Inode *inode) {
    int blk = INODE_START_BLK + ino / INODES_PER_BLOCK;
    int off = (ino % INODES_PER_BLOCK) * sizeof(Inode);
    char buf[BLOCK_SIZE];
    disk_read(blk, buf);
    memcpy(inode, buf + off, sizeof(Inode));
}

/* 写 inode */
static void write_inode(int ino, const Inode *inode) {
    int blk = INODE_START_BLK + ino / INODES_PER_BLOCK;
    int off = (ino % INODES_PER_BLOCK) * sizeof(Inode);
    char buf[BLOCK_SIZE];
    disk_read(blk, buf);
    memcpy(buf + off, inode, sizeof(Inode));
    disk_write(blk, buf);
}

/* ============ 目录操作 ============ */

/* 在目录 inode 中查找 name，返回 inode 编号，-1 表示未找到 */
static int dir_lookup(int dir_ino, const char *name) {
    Inode dir;
    read_inode(dir_ino, &dir);
    if (dir.type != TYPE_DIR) return -1;

    int nentries = dir.size / sizeof(DirEntry);
    char buf[BLOCK_SIZE];

    for (int i = 0; i < DIRECT_PTRS && i * BLOCK_SIZE < dir.size; i++) {
        if (dir.direct[i] <= 0) continue;
        disk_read(dir.direct[i], buf);
        for (int j = 0; j < (int)(BLOCK_SIZE / sizeof(DirEntry))
             && (i * (int)(BLOCK_SIZE / sizeof(DirEntry)) + j) < nentries; j++) {
            DirEntry *de = &((DirEntry *)buf)[j];
            if (de->inode_num > 0 && strcmp(de->name, name) == 0)
                return de->inode_num;
        }
    }
    return -1;
}

/* 在目录中添加条目 */
static int dir_add_entry(int dir_ino, const char *name, int new_ino) {
    Inode dir;
    read_inode(dir_ino, &dir);
    if (dir.type != TYPE_DIR) return -1;

    int nentries = dir.size / sizeof(DirEntry);
    /* 找到空闲槽或追加 */
    char buf[BLOCK_SIZE];
    int target_blk_idx = nentries / (int)(BLOCK_SIZE / sizeof(DirEntry));
    int target_off     = nentries % (int)(BLOCK_SIZE / sizeof(DirEntry));

    if (target_blk_idx >= DIRECT_PTRS) {
        printf("Directory full (max %d entries)\n",
               DIRECT_PTRS * (int)(BLOCK_SIZE / sizeof(DirEntry)));
        return -1;
    }

    /* 如果该块尚未分配 */
    if (dir.direct[target_blk_idx] <= 0) {
        int blk = alloc_block();
        if (blk < 0) return -1;
        dir.direct[target_blk_idx] = blk;
    }

    disk_read(dir.direct[target_blk_idx], buf);
    DirEntry *de = &((DirEntry *)buf)[target_off];
    strncpy(de->name, name, MAX_FILENAME - 1);
    de->name[MAX_FILENAME - 1] = '\0';
    de->inode_num = new_ino;
    disk_write(dir.direct[target_blk_idx], buf);

    dir.size += sizeof(DirEntry);
    dir.ctime = (int32_t)time(NULL);
    write_inode(dir_ino, &dir);
    return 0;
}

/* 从目录中删除条目 */
static int dir_remove_entry(int dir_ino, const char *name) {
    Inode dir;
    read_inode(dir_ino, &dir);
    if (dir.type != TYPE_DIR) return -1;

    int nentries = dir.size / sizeof(DirEntry);
    char buf[BLOCK_SIZE];

    for (int i = 0; i < DIRECT_PTRS; i++) {
        if (dir.direct[i] <= 0) continue;
        disk_read(dir.direct[i], buf);
        for (int j = 0; j < (int)(BLOCK_SIZE / sizeof(DirEntry)); j++) {
            DirEntry *de = &((DirEntry *)buf)[j];
            if (de->inode_num > 0 && strcmp(de->name, name) == 0) {
                de->inode_num = 0;
                de->name[0] = '\0';
                disk_write(dir.direct[i], buf);
                dir.ctime = (int32_t)time(NULL);
                write_inode(dir_ino, &dir);
                return 0;
            }
        }
    }
    return -1;
}

/* ============ 文件操作 ============ */

/* 获取文件数据块 (必要时分配) */
static int get_data_block(Inode *inode, int blk_idx, bool alloc) {
    if (blk_idx < DIRECT_PTRS) {
        if (inode->direct[blk_idx] <= 0 && alloc) {
            int blk = alloc_block();
            if (blk < 0) return -1;
            inode->direct[blk_idx] = blk;
            write_inode(0, inode); /* 稍后外部会写，这里先写 */
        }
        return inode->direct[blk_idx];
    }
    /* 一级间接块 */
    blk_idx -= DIRECT_PTRS;
    int per_block = BLOCK_SIZE / 4;
    if (blk_idx >= per_block) return -1;

    if (inode->indirect <= 0 && alloc) {
        int blk = alloc_block();
        if (blk < 0) return -1;
        inode->indirect = blk;
    }
    if (inode->indirect <= 0) return -1;

    int32_t ptrs[BLOCK_SIZE / 4];
    disk_read(inode->indirect, ptrs);
    if (ptrs[blk_idx] <= 0 && alloc) {
        int blk = alloc_block();
        if (blk < 0) return -1;
        ptrs[blk_idx] = blk;
        disk_write(inode->indirect, ptrs);
    }
    return ptrs[blk_idx];
}

static void free_all_blocks(Inode *inode) {
    for (int i = 0; i < DIRECT_PTRS; i++) {
        if (inode->direct[i] > 0) {
            free_block(inode->direct[i]);
            inode->direct[i] = 0;
        }
    }
    if (inode->indirect > 0) {
        int32_t ptrs[BLOCK_SIZE / 4];
        disk_read(inode->indirect, ptrs);
        for (int i = 0; i < (int)(BLOCK_SIZE / 4); i++) {
            if (ptrs[i] > 0) free_block(ptrs[i]);
        }
        free_block(inode->indirect);
        inode->indirect = 0;
    }
}

/* ============ 路径解析 ============ */

/* 简单路径解析：/a/b/c → 逐级查找，返回目标 inode，-1 表示不存在 */
static int path_resolve(const char *path, int *parent_ino) {
    if (path[0] != '/') return -1;  /* 只支持绝对路径 */

    int cur = ROOT_INODE;
    if (parent_ino) *parent_ino = cur;

    if (strcmp(path, "/") == 0) return ROOT_INODE;

    char *copy = strdup(path);
    char *tok = strtok(copy + 1, "/");
    char *last = NULL;

    while (tok) {
        last = tok;
        int next = dir_lookup(cur, tok);
        if (next < 0) {
            /* 返回父目录和最后一段名字 */
            free(copy);
            return -1;
        }
        if (parent_ino) *parent_ino = cur;
        cur = next;
        tok = strtok(NULL, "/");
    }
    free(copy);
    return cur;
}

/* 解析到父目录 + 最后一段文件名 */
static int path_to_parent(const char *path, int *parent, char *basename) {
    if (path[0] != '/') return -1;
    if (strcmp(path, "/") == 0) return -1;

    char *copy = strdup(path);
    char *slash = strrchr(copy, '/');
    *slash = '\0';
    if (copy[0] == '\0') {
        *parent = ROOT_INODE;
    } else {
        *parent = path_resolve(copy, NULL);
    }
    strncpy(basename, slash + 1, MAX_FILENAME);
    basename[MAX_FILENAME - 1] = '\0';
    free(copy);
    return (*parent >= 0) ? 0 : -1;
}

/* ============ Shell 命令 ============ */

static void cmd_mkdir(const char *path) {
    int parent;
    char name[MAX_FILENAME];
    if (path_to_parent(path, &parent, name) < 0) {
        printf("mkdir: invalid path %s\n", path);
        return;
    }
    if (dir_lookup(parent, name) >= 0) {
        printf("mkdir: '%s' already exists\n", name);
        return;
    }

    int ino = alloc_inode();
    if (ino < 0) { printf("mkdir: no free inode\n"); return; }
    int blk = alloc_block();
    if (blk < 0) { free_inode(ino); printf("mkdir: no free block\n"); return; }

    Inode inode;
    memset(&inode, 0, sizeof(Inode));
    inode.type = TYPE_DIR;
    inode.links = 2;  /* . 和父目录条目 */
    inode.size = 0;
    inode.direct[0] = blk;
    inode.ctime = (int32_t)time(NULL);
    for (int i = 1; i < DIRECT_PTRS; i++) inode.direct[i] = 0;
    inode.indirect = 0;
    write_inode(ino, &inode);

    dir_add_entry(parent, name, ino);
    /* 添加 . 和 .. 条目 */
    DirEntry dot_entries[2];
    memset(&dot_entries, 0, sizeof(dot_entries));
    strcpy(dot_entries[0].name, ".");
    dot_entries[0].inode_num = ino;
    strcpy(dot_entries[1].name, "..");
    dot_entries[1].inode_num = parent;
    disk_write(blk, dot_entries);
    Inode dir_inode;
    read_inode(ino, &dir_inode);
    dir_inode.size = 2 * (int32_t)sizeof(DirEntry);
    write_inode(ino, &dir_inode);

    printf("mkdir: created %s (inode %d)\n", path, ino);
}

static void cmd_ls(const char *path) {
    int ino = path ? path_resolve(path, NULL) : ROOT_INODE;
    if (ino < 0) { printf("ls: %s not found\n", path); return; }

    Inode dir;
    read_inode(ino, &dir);
    if (dir.type != TYPE_DIR) { printf("ls: %s is not a directory\n", path); return; }

    int nentries = dir.size / sizeof(DirEntry);
    char buf[BLOCK_SIZE];

    printf("%-28s %-6s %-6s %s\n", "Name", "Inode", "Type", "Size");
    printf("-----------------------------------------------\n");
    for (int i = 0; i < DIRECT_PTRS && i * BLOCK_SIZE < dir.size; i++) {
        if (dir.direct[i] <= 0) continue;
        disk_read(dir.direct[i], buf);
        for (int j = 0; j < (int)(BLOCK_SIZE / sizeof(DirEntry))
             && (i * (int)(BLOCK_SIZE / sizeof(DirEntry)) + j) < nentries; j++) {
            DirEntry *de = &((DirEntry *)buf)[j];
            if (de->inode_num <= 0) continue;
            Inode child;
            read_inode(de->inode_num, &child);
            printf("%-28s %-6d %-6s %d\n",
                   de->name, de->inode_num,
                   child.type == TYPE_DIR ? "DIR" : "FILE",
                   child.size);
        }
    }
}

static void cmd_create(const char *path) {
    int parent;
    char name[MAX_FILENAME];
    if (path_to_parent(path, &parent, name) < 0) {
        printf("create: invalid path %s\n", path);
        return;
    }
    if (dir_lookup(parent, name) >= 0) {
        printf("create: '%s' already exists\n", name);
        return;
    }

    int ino = alloc_inode();
    if (ino < 0) { printf("create: no free inode\n"); return; }

    Inode inode;
    memset(&inode, 0, sizeof(Inode));
    inode.type = TYPE_FILE;
    inode.links = 1;
    inode.size = 0;
    inode.ctime = (int32_t)time(NULL);
    write_inode(ino, &inode);

    dir_add_entry(parent, name, ino);
    printf("create: created %s (inode %d)\n", path, ino);
}

static void cmd_write(const char *path, const char *data) {
    int ino = path_resolve(path, NULL);
    if (ino < 0) { printf("write: %s not found\n", path); return; }

    Inode inode;
    read_inode(ino, &inode);
    if (inode.type != TYPE_FILE) { printf("write: %s is not a file\n", path); return; }

    int datalen = (int)strlen(data);
    int blk_idx = inode.size / BLOCK_SIZE;
    int blk_off = inode.size % BLOCK_SIZE;
    char buf[BLOCK_SIZE];

    int written = 0;
    while (written < datalen) {
        int blk = get_data_block(&inode, blk_idx, true);
        if (blk < 0) { printf("write: out of disk space\n"); break; }

        int to_write = datalen - written;
        if (to_write > BLOCK_SIZE - blk_off)
            to_write = BLOCK_SIZE - blk_off;

        if (blk_off > 0 || to_write < BLOCK_SIZE)
            disk_read(blk, buf);
        memcpy(buf + blk_off, data + written, to_write);
        disk_write(blk, buf);

        written += to_write;
        blk_idx++;
        blk_off = 0;
    }

    inode.size += written;
    inode.ctime = (int32_t)time(NULL);
    write_inode(ino, &inode);
    printf("write: wrote %d bytes to %s (total size=%d)\n", written, path, inode.size);
}

static void cmd_cat(const char *path) {
    int ino = path_resolve(path, NULL);
    if (ino < 0) { printf("cat: %s not found\n", path); return; }

    Inode inode;
    read_inode(ino, &inode);
    if (inode.type != TYPE_FILE) { printf("cat: %s is not a file\n", path); return; }

    char *content = malloc(inode.size + 1);
    int read_bytes = 0;
    char buf[BLOCK_SIZE];

    int nblocks = (inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (int i = 0; i < nblocks; i++) {
        int blk = get_data_block(&inode, i, false);
        if (blk <= 0) break;
        disk_read(blk, buf);
        int to_copy = inode.size - read_bytes;
        if (to_copy > BLOCK_SIZE) to_copy = BLOCK_SIZE;
        memcpy(content + read_bytes, buf, to_copy);
        read_bytes += to_copy;
    }

    content[read_bytes] = '\0';
    printf("%s\n", content);
    free(content);
}

static void cmd_rm(const char *path) {
    int parent;
    char name[MAX_FILENAME];
    if (path_to_parent(path, &parent, name) < 0) {
        printf("rm: invalid path %s\n", path);
        return;
    }
    int ino = dir_lookup(parent, name);
    if (ino < 0) { printf("rm: %s not found\n", path); return; }

    Inode inode;
    read_inode(ino, &inode);

    /* 目录必须为空 */
    if (inode.type == TYPE_DIR) {
        int nentries = inode.size / sizeof(DirEntry);
        int valid = 0;
        char buf[BLOCK_SIZE];
        for (int i = 0; i < DIRECT_PTRS && i * BLOCK_SIZE < inode.size; i++) {
            if (inode.direct[i] <= 0) continue;
            disk_read(inode.direct[i], buf);
            for (int j = 0; j < (int)(BLOCK_SIZE / sizeof(DirEntry))
                 && (i * (int)(BLOCK_SIZE / sizeof(DirEntry)) + j) < nentries; j++) {
                DirEntry *de = &((DirEntry *)buf)[j];
                if (de->inode_num > 0 && strcmp(de->name, ".") != 0
                    && strcmp(de->name, "..") != 0)
                    valid++;
            }
        }
        if (valid > 0) { printf("rm: %s is not empty\n", path); return; }
    }

    free_all_blocks(&inode);
    free_inode(ino);
    dir_remove_entry(parent, name);
    printf("rm: removed %s\n", path);
}

static void cmd_df(void) {
    int total_data = NUM_BLOCKS - DATA_START_BLK;
    printf("Filesystem usage:\n");
    printf("  Total blocks : %d\n", NUM_BLOCKS);
    printf("  Block size   : %d B\n", BLOCK_SIZE);
    printf("  Total inodes : %d\n", NUM_INODES);
    printf("  Free blocks  : %d / %d\n", g_sb.free_blocks, total_data);
    printf("  Free inodes  : %d / %d\n", g_sb.free_inodes, NUM_INODES);
    printf("  Disk size    : %d KB\n", NUM_BLOCKS * BLOCK_SIZE / 1024);
}

/* ============ 格式化 ============ */

static void fs_format(void) {
    /* 初始化 superblock */
    memset(&g_sb, 0, sizeof(g_sb));
    g_sb.magic       = 0x4F534CAB;
    g_sb.num_blocks  = NUM_BLOCKS;
    g_sb.num_inodes  = NUM_INODES;
    g_sb.free_blocks = NUM_BLOCKS - DATA_START_BLK;
    g_sb.free_inodes = NUM_INODES;
    g_sb.inode_start = INODE_START_BLK;
    g_sb.data_start  = DATA_START_BLK;
    disk_write(0, &g_sb);

    /* 初始化 inode 位图 */
    memset(g_inode_bmp, 0, BLOCK_SIZE);
    disk_write(INODE_BITMAP_BLK, g_inode_bmp);

    /* 初始化数据块位图 */
    memset(g_block_bmp, 0, BLOCK_SIZE);
    /* 前 DATA_START_BLK 块标记为已用 */
    for (int i = 0; i < DATA_START_BLK; i++)
        bitmap_set(g_block_bmp, i, true);
    disk_write(DATA_BITMAP_BLK, g_block_bmp);

    /* 清零 inode 表 */
    for (int i = 0; i < INODE_BLOCKS; i++)
        disk_zero(INODE_START_BLK + i);

    /* 创建根目录 / */
    int root_ino = alloc_inode();
    int root_blk = alloc_block();

    Inode root;
    memset(&root, 0, sizeof(root));
    root.type = TYPE_DIR;
    root.links = 2;
    root.size = 2 * sizeof(DirEntry);
    root.direct[0] = root_blk;
    root.ctime = (int32_t)time(NULL);
    write_inode(root_ino, &root);

    DirEntry dot[2];
    memset(dot, 0, sizeof(dot));
    strcpy(dot[0].name, ".");
    dot[0].inode_num = root_ino;
    strcpy(dot[1].name, "..");
    dot[1].inode_num = root_ino;
    disk_write(root_blk, dot);

    /* 更新 superblock */
    g_sb.free_blocks--;
    g_sb.free_inodes--;
    disk_write(0, &g_sb);

    printf("Filesystem formatted: %d blocks, %d inodes\n", NUM_BLOCKS, NUM_INODES);
}

static void fs_mount(void) {
    /* 读入 superblock */
    disk_read(0, &g_sb);
    disk_read(INODE_BITMAP_BLK, g_inode_bmp);
    disk_read(DATA_BITMAP_BLK, g_block_bmp);

    if (g_sb.magic != 0x4F534CAB) {
        printf("Invalid filesystem or not formatted. Formatting...\n");
        fs_format();
    }
}

/* ============ Shell ============ */

static void show_help(void) {
    printf("\nCommands:\n");
    printf("  mkdir <path>         Create directory\n");
    printf("  ls [path]            List directory\n");
    printf("  create <path>        Create empty file\n");
    printf("  write <path> <data>  Write data to file\n");
    printf("  cat <path>           Read file\n");
    printf("  rm <path>            Remove file or empty directory\n");
    printf("  df                   Show filesystem usage\n");
    printf("  format               Format (destroy all data!)\n");
    printf("  help                 This help\n");
    printf("  quit                 Exit\n\n");
}

static void shell(void) {
    char line[1024];
    printf("\n===== File System Shell =====\n");
    show_help();

    while (1) {
        printf("fs> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;

        /* 去除末尾换行 */
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        char cmd[32], arg1[512], arg2[512];
        arg1[0] = arg2[0] = '\0';
        int n = sscanf(line, "%31s %511s %511[^\n]", cmd, arg1, arg2);

        if (n < 1) continue;

        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            break;
        } else if (strcmp(cmd, "help") == 0) {
            show_help();
        } else if (strcmp(cmd, "format") == 0) {
            printf("Are you sure? (y/N): ");
            char ans[8];
            if (fgets(ans, sizeof ans, stdin) && (ans[0] == 'y' || ans[0] == 'Y'))
                fs_format();
        } else if (strcmp(cmd, "mkdir") == 0 && n >= 2) {
            cmd_mkdir(arg1);
        } else if (strcmp(cmd, "ls") == 0) {
            cmd_ls(n >= 2 ? arg1 : "/");
        } else if (strcmp(cmd, "create") == 0 && n >= 2) {
            cmd_create(arg1);
        } else if (strcmp(cmd, "write") == 0 && n >= 3) {
            cmd_write(arg1, arg2);
        } else if (strcmp(cmd, "cat") == 0 && n >= 2) {
            cmd_cat(arg1);
        } else if (strcmp(cmd, "rm") == 0 && n >= 2) {
            cmd_rm(arg1);
        } else if (strcmp(cmd, "df") == 0) {
            cmd_df();
        } else {
            printf("Unknown command. Type 'help'.\n");
        }
    }
}

/* ============ 批量模式 ============ */

static void run_script(const char *script_path) {
    FILE *fp = fopen(script_path, "r");
    if (!fp) { perror(script_path); return; }
    char line[1024];
    while (fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;
        printf("fs> %s\n", line);

        char cmd[32], arg1[512], arg2[512];
        arg1[0] = arg2[0] = '\0';
        int n = sscanf(line, "%31s %511s %511[^\n]", cmd, arg1, arg2);

        if (strcmp(cmd, "mkdir") == 0 && n >= 2) cmd_mkdir(arg1);
        else if (strcmp(cmd, "ls") == 0) cmd_ls(n >= 2 ? arg1 : "/");
        else if (strcmp(cmd, "create") == 0 && n >= 2) cmd_create(arg1);
        else if (strcmp(cmd, "write") == 0 && n >= 3) cmd_write(arg1, arg2);
        else if (strcmp(cmd, "cat") == 0 && n >= 2) cmd_cat(arg1);
        else if (strcmp(cmd, "rm") == 0 && n >= 2) cmd_rm(arg1);
        else if (strcmp(cmd, "df") == 0) cmd_df();
        else if (strcmp(cmd, "format") == 0) fs_format();
        else printf("Unknown: %s\n", cmd);
    }
    fclose(fp);
}

int main(int argc, char **argv) {
    const char *disk_path = "disk.img";
    const char *script = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
            disk_path = argv[++i];
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
            script = argv[++i];
        else if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [-d disk_img] [-f script]\n", argv[0]);
            return 0;
        }
    }

    g_disk = fopen(disk_path, "r+b");
    if (!g_disk) {
        /* 创建新磁盘映像 */
        g_disk = fopen(disk_path, "w+b");
        if (!g_disk) { perror(disk_path); return 1; }
        /* 预分配空间 */
        fseek(g_disk, NUM_BLOCKS * BLOCK_SIZE - 1, SEEK_SET);
        fputc(0, g_disk);
        fflush(g_disk);
    }

    fs_mount();

    if (script)
        run_script(script);
    else
        shell();

    fclose(g_disk);
    printf("Bye.\n");
    return 0;
}
