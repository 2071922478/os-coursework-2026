/*
 * paging.c — 页面置换算法模拟器
 *
 * 支持算法: FIFO / LRU / OPT (最优置换作为基线)
 * 输入: 页面引用串 + 可用帧数
 * 输出: 每步帧状态 + 缺页率
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#define MAX_REFS 256
#define MAX_FRAMES 64

typedef struct {
    int  page;        /* -1 = 空闲 */
    int  load_time;   /* 装入时刻 (FIFO 用) */
    int  last_use;    /* 最近使用时刻 (LRU 用) */
} Frame;

typedef struct {
    Frame frames[MAX_FRAMES];
    int   nframes;
    int   faults;
    int   total;
} Sim;

static void sim_init(Sim *s, int nframes) {
    s->nframes = nframes;
    s->faults = 0;
    s->total  = 0;
    for (int i = 0; i < nframes; i++) {
        s->frames[i].page = -1;
        s->frames[i].load_time = -1;
        s->frames[i].last_use = -1;
    }
}

static void show_frames(Sim *s, int time, int ref, bool fault) {
    printf("t=%-3d ref=%-3d ", time, ref);
    for (int i = 0; i < s->nframes; i++) {
        if (s->frames[i].page >= 0)
            printf("[%2d]", s->frames[i].page);
        else
            printf("[  ]");
    }
    printf(" %c", fault ? 'F' : ' ');
    printf("\n");
}

static int find_page(Sim *s, int page) {
    for (int i = 0; i < s->nframes; i++)
        if (s->frames[i].page == page) return i;
    return -1;
}

static int find_free(Sim *s) {
    for (int i = 0; i < s->nframes; i++)
        if (s->frames[i].page == -1) return i;
    return -1;
}

/* ---- FIFO ---- */

static void run_fifo(Sim *s, int *refs, int nrefs, bool verbose) {
    sim_init(s, s->nframes);
    int fifo_ptr = 0;

    printf("\n===== FIFO (frames=%d) =====\n", s->nframes);
    if (verbose) printf("%-16s %s\n", "", "Frame State");

    for (int t = 0; t < nrefs; t++) {
        int r = refs[t];
        s->total++;
        int pos = find_page(s, r);
        if (pos >= 0) {
            if (verbose) show_frames(s, t, r, false);
            continue;
        }
        s->faults++;
        int free = find_free(s);
        if (free >= 0) {
            s->frames[free].page = r;
            s->frames[free].load_time = t;
        } else {
            /* 找最早装入的 */
            int oldest = 0;
            for (int i = 1; i < s->nframes; i++)
                if (s->frames[i].load_time < s->frames[oldest].load_time)
                    oldest = i;
            s->frames[oldest].page = r;
            s->frames[oldest].load_time = t;
        }
        if (verbose) show_frames(s, t, r, true);
    }
    printf("Page faults: %d / %d  Rate: %.1f%%\n",
           s->faults, s->total, 100.0 * s->faults / s->total);
}

/* ---- LRU ---- */

static void run_lru(Sim *s, int *refs, int nrefs, bool verbose) {
    sim_init(s, s->nframes);

    printf("\n===== LRU (frames=%d) =====\n", s->nframes);
    if (verbose) printf("%-16s %s\n", "", "Frame State");

    for (int t = 0; t < nrefs; t++) {
        int r = refs[t];
        s->total++;
        int pos = find_page(s, r);
        if (pos >= 0) {
            s->frames[pos].last_use = t;
            if (verbose) show_frames(s, t, r, false);
            continue;
        }
        s->faults++;
        int free = find_free(s);
        if (free >= 0) {
            s->frames[free].page = r;
            s->frames[free].last_use = t;
        } else {
            /* 找最久未使用的 */
            int lru = 0;
            for (int i = 1; i < s->nframes; i++)
                if (s->frames[i].last_use < s->frames[lru].last_use)
                    lru = i;
            s->frames[lru].page = r;
            s->frames[lru].last_use = t;
        }
        if (verbose) show_frames(s, t, r, true);
    }
    printf("Page faults: %d / %d  Rate: %.1f%%\n",
           s->faults, s->total, 100.0 * s->faults / s->total);
}

/* ---- OPT (最优置换) ---- */

static int next_use(int page, int *refs, int nrefs, int from) {
    for (int i = from; i < nrefs; i++)
        if (refs[i] == page) return i;
    return INT_MAX;  /* 永不再用 */
}

static void run_opt(Sim *s, int *refs, int nrefs, bool verbose) {
    sim_init(s, s->nframes);

    printf("\n===== OPT (frames=%d) =====\n", s->nframes);
    if (verbose) printf("%-16s %s\n", "", "Frame State");

    for (int t = 0; t < nrefs; t++) {
        int r = refs[t];
        s->total++;
        int pos = find_page(s, r);
        if (pos >= 0) {
            if (verbose) show_frames(s, t, r, false);
            continue;
        }
        s->faults++;
        int free = find_free(s);
        if (free >= 0) {
            s->frames[free].page = r;
        } else {
            /* 找最远将来才使用的（或永不再用） */
            int victim = 0, farthest = next_use(s->frames[0].page, refs, nrefs, t + 1);
            for (int i = 1; i < s->nframes; i++) {
                int nu = next_use(s->frames[i].page, refs, nrefs, t + 1);
                if (nu > farthest) { farthest = nu; victim = i; }
            }
            s->frames[victim].page = r;
        }
        if (verbose) show_frames(s, t, r, true);
    }
    printf("Page faults: %d / %d  Rate: %.1f%%\n",
           s->faults, s->total, 100.0 * s->faults / s->total);
}

/* ---- 加载引用串 ---- */

static int load_refs(const char *s, int *refs) {
    int n = 0;
    char *copy = strdup(s);
    char *tok = strtok(copy, " ,\t\n");
    while (tok && n < MAX_REFS) {
        refs[n++] = atoi(tok);
        tok = strtok(NULL, " ,\t\n");
    }
    free(copy);
    return n;
}

static int load_refs_file(const char *path, int *refs) {
    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return -1; }
    char line[1024];
    if (!fgets(line, sizeof line, fp)) { fclose(fp); return -1; }
    fclose(fp);
    /* 跳过注释 */
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '#') {
        /* 读下一行 */
        fp = fopen(path, "r");
        if (!fp) return -1;
        while (fgets(line, sizeof line, fp)) {
            p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p != '#' && *p != '\n') break;
        }
        fclose(fp);
    }
    return load_refs(line, refs);
}

int main(int argc, char **argv) {
    int refs[MAX_REFS];
    int nrefs = 0;
    int frames = 3;
    int verbose = 1;

    /* 命令行: paging [-f file] [-n frames] [-r "1 2 3 4 ..."] [-q] */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
            nrefs = load_refs_file(argv[++i], refs);
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc)
            nrefs = load_refs(argv[++i], refs);
        else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
            frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "-q") == 0)
            verbose = 0;
        else if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [-f file] [-r \"refs...\"] [-n frames] [-q quiet]\n", argv[0]);
            return 0;
        }
    }

    if (nrefs <= 0) {
        printf("No reference string. Use -r \"1 2 3 4\" or -f file\n");
        printf("Interactive: enter reference string (space-separated): ");
        char line[1024];
        if (!fgets(line, sizeof line, stdin)) return 1;
        nrefs = load_refs(line, refs);
        printf("Frames: ");
        if (scanf("%d", &frames) != 1) frames = 3;
    }

    if (nrefs <= 0 || frames <= 0) {
        fprintf(stderr, "Invalid input\n");
        return 1;
    }

    printf("Reference string (%d pages): ", nrefs);
    for (int i = 0; i < nrefs; i++) printf("%d ", refs[i]);
    printf("\nFrames: %d\n", frames);

    Sim sim;
    sim.nframes = frames;

    run_fifo(&sim, refs, nrefs, verbose);
    run_lru(&sim, refs, nrefs, verbose);
    run_opt(&sim, refs, nrefs, verbose);

    return 0;
}
