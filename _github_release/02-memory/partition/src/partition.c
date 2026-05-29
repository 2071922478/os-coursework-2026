/*
 * partition.c — 动态分区分配模拟器
 *
 * 支持算法: First Fit (FF) / Best Fit (BF) / Worst Fit (WF)
 * 操作: 分配 (ALLOC) / 释放 (FREE) / 显示状态 / 紧凑 (COMPACT)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_BLOCKS 128
#define MAX_NAME   16

typedef struct {
    char name[MAX_NAME];
    int  start;
    int  size;
    bool free;       /* true = hole */
} Block;

typedef struct {
    Block blocks[MAX_BLOCKS];
    int   count;
    int   total_mem;
} Memory;

/* ---- 工具 ---- */

static void mem_init(Memory *m, int total) {
    m->total_mem = total;
    m->count = 1;
    snprintf(m->blocks[0].name, MAX_NAME, "FREE");
    m->blocks[0].start = 0;
    m->blocks[0].size  = total;
    m->blocks[0].free  = true;
}

static void mem_show(Memory *m) {
    printf("\n--- Memory Map (total=%d) ---\n", m->total_mem);
    printf("%-8s %-8s %-8s %s\n", "Block", "Start", "Size", "Status");
    int free_total = 0, used_total = 0, hole_count = 0;
    for (int i = 0; i < m->count; i++) {
        Block *b = &m->blocks[i];
        printf("%-8s %-8d %-8d %s\n",
               b->name, b->start, b->size,
               b->free ? "FREE" : "ALLOCATED");
        if (b->free) { free_total += b->size; hole_count++; }
        else         used_total += b->size;
    }
    printf("Used: %d  Free: %d  Holes: %d  Fragmentation: %.1f%%\n",
           used_total, free_total, hole_count,
           free_total > 0 ? 100.0 * (free_total - (hole_count > 0 ? free_total : 0)) / free_total : 0);
    /* External fragmentation: largest hole vs total free */
    int largest = 0;
    for (int i = 0; i < m->count; i++)
        if (m->blocks[i].free && m->blocks[i].size > largest)
            largest = m->blocks[i].size;
    printf("Largest free block: %d  ", largest);
    if (free_total > 0 && hole_count > 1)
        printf("External fragmentation: %.1f%%",
               100.0 * (free_total - largest) / free_total);
    printf("\n");
}

static void mem_compact(Memory *m) {
    /* 将所有已分配块向低地址端移动，合并所有空闲块 */
    Block new_blocks[MAX_BLOCKS];
    int nc = 0, offset = 0;

    /* 先收集所有已分配块（保持相对顺序） */
    for (int i = 0; i < m->count; i++) {
        if (!m->blocks[i].free) {
            new_blocks[nc] = m->blocks[i];
            new_blocks[nc].start = offset;
            offset += new_blocks[nc].size;
            nc++;
        }
    }
    /* 末尾一个空闲块 */
    if (offset < m->total_mem) {
        snprintf(new_blocks[nc].name, MAX_NAME, "FREE");
        new_blocks[nc].start = offset;
        new_blocks[nc].size  = m->total_mem - offset;
        new_blocks[nc].free  = true;
        nc++;
    }
    m->count = nc;
    memcpy(m->blocks, new_blocks, sizeof(Block) * nc);
    printf("Compacted: moved all allocations to low address, merged free space.\n");
}

/* 在空闲块 i 中分配 size */
static void split_block(Memory *m, int i, int size, const char *name) {
    Block *b = &m->blocks[i];
    if (b->size == size) {
        /* 恰好匹配 */
        snprintf(b->name, MAX_NAME, "%s", name);
        b->free = false;
    } else {
        /* 分裂：后半部分分配给新进程，前半部分保持空闲 */
        for (int j = m->count; j > i; j--)
            m->blocks[j] = m->blocks[j - 1];
        m->count++;
        m->blocks[i + 1] = m->blocks[i];
        snprintf(m->blocks[i + 1].name, MAX_NAME, "%s", name);
        m->blocks[i + 1].start = b->start;
        m->blocks[i + 1].size  = size;
        m->blocks[i + 1].free  = false;
        m->blocks[i].start += size;
        m->blocks[i].size  -= size;
        /* 保持空闲 */
    }
}

/* ---- 分配算法 ---- */

typedef int (*FitFunc)(Memory *m, int size);

static int fit_ff(Memory *m, int size) {
    for (int i = 0; i < m->count; i++)
        if (m->blocks[i].free && m->blocks[i].size >= size)
            return i;
    return -1;
}

static int fit_bf(Memory *m, int size) {
    int best = -1, best_size = m->total_mem + 1;
    for (int i = 0; i < m->count; i++) {
        if (m->blocks[i].free && m->blocks[i].size >= size
            && m->blocks[i].size < best_size) {
            best = i;
            best_size = m->blocks[i].size;
        }
    }
    return best;
}

static int fit_wf(Memory *m, int size) {
    int worst = -1, worst_size = -1;
    for (int i = 0; i < m->count; i++) {
        if (m->blocks[i].free && m->blocks[i].size >= size
            && m->blocks[i].size > worst_size) {
            worst = i;
            worst_size = m->blocks[i].size;
        }
    }
    return worst;
}

static const char *fit_name(int alg) {
    switch (alg) { case 0: return "FF"; case 1: return "BF"; case 2: return "WF"; }
    return "?";
}

static int mem_alloc(Memory *m, int size, const char *name, int alg) {
    FitFunc funcs[] = { fit_ff, fit_bf, fit_wf };
    int idx = funcs[alg](m, size);
    if (idx < 0) {
        printf("ALLOC %s(%d) via %s: FAILED (no suitable hole)\n", name, size, fit_name(alg));
        return -1;
    }
    printf("ALLOC %s(%d) via %s: placed at %d\n", name, size, fit_name(alg), m->blocks[idx].start);
    split_block(m, idx, size, name);
    return 0;
}

static int mem_free(Memory *m, const char *name) {
    for (int i = 0; i < m->count; i++) {
        if (!m->blocks[i].free && strcmp(m->blocks[i].name, name) == 0) {
            m->blocks[i].free = true;
            snprintf(m->blocks[i].name, MAX_NAME, "FREE");
            printf("FREE %s: released at %d, size %d\n", name, m->blocks[i].start, m->blocks[i].size);
            /* 与相邻空闲块合并 */
            if (i + 1 < m->count && m->blocks[i + 1].free) {
                m->blocks[i].size += m->blocks[i + 1].size;
                for (int j = i + 1; j < m->count - 1; j++)
                    m->blocks[j] = m->blocks[j + 1];
                m->count--;
            }
            if (i > 0 && m->blocks[i - 1].free) {
                m->blocks[i - 1].size += m->blocks[i].size;
                for (int j = i; j < m->count - 1; j++)
                    m->blocks[j] = m->blocks[j + 1];
                m->count--;
            }
            return 0;
        }
    }
    printf("FREE %s: not found\n", name);
    return -1;
}

/* ---- 用例文件格式 ----
   每行一条指令:
     alloc <name> <size>
     free <name>
     compact
     show
     # 注释
*/

static void run_case_file(Memory *m, int alg, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return; }
    char line[256];
    printf("\n========== Running %s with %s ==========\n", path, fit_name(alg));
    while (fgets(line, sizeof line, fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#' || *p == '\n') continue;
        char cmd[16], arg1[MAX_NAME];
        int arg2;
        if (sscanf(p, "%15s %15s %d", cmd, arg1, &arg2) >= 2
            && strcmp(cmd, "alloc") == 0) {
            mem_alloc(m, arg2, arg1, alg);
            mem_show(m);
        } else if (sscanf(p, "%15s %15s", cmd, arg1) >= 2
                   && strcmp(cmd, "free") == 0) {
            mem_free(m, arg1);
            mem_show(m);
        } else if (strncmp(p, "compact", 7) == 0) {
            mem_compact(m);
            mem_show(m);
        } else if (strncmp(p, "show", 4) == 0) {
            mem_show(m);
        } else {
            printf("Unknown: %s", line);
        }
    }
    fclose(fp);
    printf("========== %s with %s done ==========\n\n", path, fit_name(alg));
}

/* ---- 交互式 ---- */

static void interactive(Memory *m) {
    int alg = 0;
    while (1) {
        printf("\n===== Dynamic Partition (alg=%s) =====\n", fit_name(alg));
        printf("1) Set algorithm (0=FF 1=BF 2=WF, current=%s)\n", fit_name(alg));
        printf("2) ALLOC <name> <size>\n3) FREE <name>\n4) COMPACT\n5) Show\n");
        printf("6) Run case file\n7) Reset memory\n0) Quit\nChoose: ");
        int c;
        if (scanf("%d", &c) != 1) break;
        switch (c) {
            case 1: printf("Algorithm (0=FF 1=BF 2=WF): ");
                    if (scanf("%d", &alg) != 1) alg = 0; break;
            case 2: {
                char name[MAX_NAME]; int sz;
                printf("name size: ");
                if (scanf("%15s %d", name, &sz) != 2) break;
                mem_alloc(m, sz, name, alg);
                mem_show(m);
                break;
            }
            case 3: {
                char name[MAX_NAME];
                printf("name: ");
                if (scanf("%15s", name) != 1) break;
                mem_free(m, name);
                mem_show(m);
                break;
            }
            case 4: mem_compact(m); mem_show(m); break;
            case 5: mem_show(m); break;
            case 6: {
                char path[256];
                printf("case file path: ");
                if (scanf("%255s", path) != 1) break;
                Memory tmp = *m;
                run_case_file(&tmp, alg, path);
                break;
            }
            case 7: mem_init(m, m->total_mem); printf("Reset.\n"); break;
            case 0: return;
        }
    }
}

int main(int argc, char **argv) {
    Memory mem;
    mem_init(&mem, 1024);
    int alg = 0;

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
                mem_init(&mem, atoi(argv[++i]));
            else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
                if (!strcmp(argv[++i], "bf")) alg = 1;
                else if (!strcmp(argv[i], "wf")) alg = 2;
                else alg = 0;
            } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
                Memory tmp = mem;
                run_case_file(&tmp, alg, argv[++i]);
                return 0;
            } else if (strcmp(argv[i], "-h") == 0) {
                printf("Usage: %s [-m total_mem] [-a ff|bf|wf] [-f case_file]\n", argv[0]);
                return 0;
            }
        }
    }

    interactive(&mem);
    return 0;
}
