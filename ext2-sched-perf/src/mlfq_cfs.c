/*
 * mlfq_cfs.c — MLFQ 与 CFS (简化版) 调度算法模拟器
 *
 * 算法:
 *   MLFQ — 多级反馈队列:
 *     3 级队列, 时间片分别为 2/4/8
 *     用完时间片的进程降级, 每 30 时间单位 boost 到顶
 *
 *   CFS — 完全公平调度 (简化 vruntime):
 *     每次调度选择 vruntime 最小的进程
 *     vruntime += 实际运行时间 * (nice0_weight / 进程权重)
 *
 * 输入格式兼容 01-scheduler 的用例文件
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#define MAX_PROCS 64
#define MAX_NAME  16
#define MAX_TIME  8192

typedef struct {
    char  name[MAX_NAME];
    int   arrival;
    int   burst;
    int   priority;      /* nice-like: 越小越优先 (越小 weight 越大) */
    /* runtime */
    int   remaining;
    int   start;
    int   finish;
    bool  done;
} Process;

/* ============ 工具 ============ */

static void reset_runtime(Process *ps, int n) {
    for (int i = 0; i < n; ++i) {
        ps[i].remaining = ps[i].burst;
        ps[i].start = -1;
        ps[i].finish = 0;
        ps[i].done = false;
    }
}

static void gantt_push(char *gantt_pid[MAX_TIME], int *glen,
                       const char *who, int len) {
    for (int i = 0; i < len && *glen < MAX_TIME; ++i)
        gantt_pid[(*glen)++] = strdup(who);
}

static void print_gantt(char *gantt_pid[MAX_TIME], int total) {
    if (total == 0) { printf("(empty)\n"); return; }
    printf("\nGantt chart:\n ");
    int i = 0;
    while (i < total) {
        int j = i;
        while (j < total && strcmp(gantt_pid[j], gantt_pid[i]) == 0) j++;
        int width = (j - i) * 2 + 2;
        for (int k = 0; k < width; ++k) putchar('-');
        putchar(' ');
        i = j;
    }
    printf("\n|");
    i = 0;
    while (i < total) {
        int j = i;
        while (j < total && strcmp(gantt_pid[j], gantt_pid[i]) == 0) j++;
        int width = (j - i) * 2 + 2;
        int pad = (width - (int)strlen(gantt_pid[i])) / 2;
        for (int k = 0; k < pad; ++k) putchar(' ');
        printf("%s", gantt_pid[i]);
        for (int k = pad + (int)strlen(gantt_pid[i]); k < width; ++k) putchar(' ');
        putchar('|');
        i = j;
    }
    printf("\n ");
    i = 0;
    while (i < total) {
        int j = i;
        while (j < total && strcmp(gantt_pid[j], gantt_pid[i]) == 0) j++;
        int width = (j - i) * 2 + 2;
        for (int k = 0; k < width; ++k) putchar('-');
        putchar(' ');
        i = j;
    }
    printf("\n0");
    int t = 0;
    i = 0;
    while (i < total) {
        int j = i;
        while (j < total && strcmp(gantt_pid[j], gantt_pid[i]) == 0) j++;
        t += (j - i);
        int width = (j - i) * 2 + 2;
        for (int k = 0; k < width - 2; ++k) putchar(' ');
        printf("%3d", t);
        i = j;
    }
    printf("\n");
}

static void free_gantt(char *gantt_pid[MAX_TIME], int total) {
    for (int i = 0; i < total; ++i) free(gantt_pid[i]);
}

static void print_stats(Process *ps, int n) {
    double sum_wait = 0, sum_turn = 0;
    printf("\n%-6s %-8s %-7s %-9s %-6s %-7s %-8s %-10s\n",
           "PID", "Arrival", "Burst", "Priority", "Start", "Finish",
           "Waiting", "Turnaround");
    for (int i = 0; i < n; ++i) {
        int turn = ps[i].finish - ps[i].arrival;
        int wait = turn - ps[i].burst;
        sum_wait += wait; sum_turn += turn;
        printf("%-6s %-8d %-7d %-9d %-6d %-7d %-8d %-10d\n",
               ps[i].name, ps[i].arrival, ps[i].burst, ps[i].priority,
               ps[i].start, ps[i].finish, wait, turn);
    }
    printf("\nAverage waiting time   : %.2f\n", sum_wait / n);
    printf("Average turnaround time: %.2f\n", sum_turn / n);
}

/* ============ MLFQ ============ */

#define MLFQ_LEVELS   3
#define BOOST_INTERVAL 30

static void run_mlfq(Process *ps, int n) {
    reset_runtime(ps, n);
    char *gantt[MAX_TIME] = {0};
    int   glen = 0;
    int   t = 0, finished = 0;

    int queue[MLFQ_LEVELS][MAX_PROCS * 8];
    int qhead[MLFQ_LEVELS], qtail[MLFQ_LEVELS];
    int quanta[MLFQ_LEVELS] = {2, 4, 8};
    int level[MAX_PROCS];         /* 每个进程当前在哪一级 */
    int used_slice[MAX_PROCS];    /* 当前时间片已用 */

    for (int l = 0; l < MLFQ_LEVELS; l++)
        qhead[l] = qtail[l] = 0;

    /* 所有进程从最高优先级 (level 0) 开始 */
    for (int i = 0; i < n; i++) {
        level[i] = 0;
        used_slice[i] = 0;
    }

    /* 按到达时间排序 */
    int order[MAX_PROCS];
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (ps[order[j]].arrival < ps[order[i]].arrival) {
                int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }
    int scan = 0;
    int last_boost = 0;

    while (finished < n) {
        /* 周期性 boost: 全部踢回最高优先级 */
        if (t - last_boost >= BOOST_INTERVAL) {
            for (int l = 0; l < MLFQ_LEVELS; l++) {
                for (int qi = qhead[l]; qi < qtail[l]; qi++) {
                    int idx = queue[l][qi];
                    level[idx] = 0;
                    used_slice[idx] = 0;
                }
                /* 把低优先级队列的内容移到最高优先级 */
                if (l > 0) {
                    for (int qi = qhead[l]; qi < qtail[l]; qi++)
                        queue[0][qtail[0]++] = queue[l][qi];
                    qhead[l] = qtail[l] = 0;
                }
            }
            last_boost = t;
        }

        /* 新到达的进程入最高优先级队列 */
        while (scan < n && ps[order[scan]].arrival <= t) {
            int idx = order[scan++];
            if (!ps[idx].done) {
                queue[0][qtail[0]++] = idx;
                level[idx] = 0;
                used_slice[idx] = 0;
            }
        }

        /* 选最高优先级非空队列的队首 */
        int chosen = -1, chosen_lvl = -1;
        for (int l = 0; l < MLFQ_LEVELS; l++) {
            if (qhead[l] < qtail[l]) {
                chosen = queue[l][qhead[l]++];
                chosen_lvl = l;
                break;
            }
        }

        if (chosen < 0) {
            gantt_push(gantt, &glen, "idle", 1);
            t++;
            continue;
        }

        if (ps[chosen].start < 0) ps[chosen].start = t;

        int slice = quanta[chosen_lvl];
        if (slice > ps[chosen].remaining) slice = ps[chosen].remaining;

        gantt_push(gantt, &glen, ps[chosen].name, slice);
        ps[chosen].remaining -= slice;
        t += slice;

        /* 在 slice 期间新到达的 */
        while (scan < n && ps[order[scan]].arrival <= t) {
            int idx = order[scan++];
            if (!ps[idx].done) {
                queue[0][qtail[0]++] = idx;
                level[idx] = 0;
                used_slice[idx] = 0;
            }
        }

        if (ps[chosen].remaining == 0) {
            ps[chosen].finish = t;
            ps[chosen].done = true;
            finished++;
        } else {
            /* 降级 (除非已是最低级) */
            int next_lvl = chosen_lvl + 1;
            if (next_lvl >= MLFQ_LEVELS) next_lvl = MLFQ_LEVELS - 1;
            level[chosen] = next_lvl;
            used_slice[chosen] = 0;
            queue[next_lvl][qtail[next_lvl]++] = chosen;
        }
    }

    printf("\n===== MLFQ (3 levels, quanta=2/4/8, boost=%d) =====\n", BOOST_INTERVAL);
    print_gantt(gantt, glen);
    print_stats(ps, n);
    free_gantt(gantt, glen);
}

/* ============ CFS (简化 vruntime) ============ */

/* nice 值 → 权重 (Linux 内核简化映射) */
static int nice_to_weight(int nice) {
    /* 简化: nice=-20 → 88761, nice=0 → 1024, nice=19 → 15 */
    static const int weights[] = {
        88761, 71755, 56483, 46273, 36291,
        29154, 23254, 18705, 14949, 11916,
         9548,  7620,  6100,  4904,  3906,
         3121,  2501,  1991,  1586,  1277,
         1024,   820,   655,   526,   423,
          335,   272,   215,   172,   137,
          110,    87,    70,    56,    45,
           36,    29,    23,    18,    15
    };
    int idx = nice + 20;  /* nice 范围假设 -20..+19 */
    if (idx < 0) idx = 0;
    if (idx >= 40) idx = 39;
    return weights[idx];
}

static void run_cfs(Process *ps, int n) {
    reset_runtime(ps, n);
    char *gantt[MAX_TIME] = {0};
    int   glen = 0;
    int   t = 0, finished = 0;

    double vruntime[MAX_PROCS] = {0.0};
    const int nice0_weight = nice_to_weight(0);
    int weights[MAX_PROCS];

    for (int i = 0; i < n; i++) {
        /* 将 priority 字段映射为 nice 值 (假设 -10 到 +10) */
        int nice = (ps[i].priority - 5) * 2; /* 映射到大概 -10..+10 */
        if (nice < -20) nice = -20;
        if (nice > 19) nice = 19;
        weights[i] = nice_to_weight(nice);
    }

    /* 按到达时间排序 */
    int order[MAX_PROCS];
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (ps[order[j]].arrival < ps[order[i]].arrival) {
                int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }
    int scan = 0;

    while (finished < n) {
        /* 选已到达的进程中 vruntime 最小的 */
        int best = -1;
        double best_vr = 1e18;
        for (int i = 0; i < n; i++) {
            if (ps[i].done || ps[i].arrival > t) continue;
            if (vruntime[i] < best_vr || (vruntime[i] == best_vr && weights[i] > weights[best])) {
                best = i;
                best_vr = vruntime[i];
            }
        }

        if (best < 0) {
            gantt_push(gantt, &glen, "idle", 1);
            t++;
            continue;
        }

        if (ps[best].start < 0) ps[best].start = t;

        /* CFS 时间片 = 理想运行时间 (简化为 1 单位) */
        int slice = 1;
        if (slice > ps[best].remaining) slice = ps[best].remaining;

        gantt_push(gantt, &glen, ps[best].name, slice);
        ps[best].remaining -= slice;
        t += slice;

        /* vruntime 更新: 实际运行时间 * (nice0 / weight) */
        vruntime[best] += (double)slice * nice0_weight / weights[best];

        if (ps[best].remaining == 0) {
            ps[best].finish = t;
            ps[best].done = true;
            finished++;
        }
    }

    printf("\n===== CFS (simplified vruntime) =====\n");
    print_gantt(gantt, glen);
    print_stats(ps, n);
    free_gantt(gantt, glen);
}

/* ============ 输入 ============ */

static int load_from_file(const char *path, Process *ps) {
    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return -1; }
    char line[256];
    int n = 0;
    while (fgets(line, sizeof line, fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#') continue;
        if (n >= MAX_PROCS) break;
        if (sscanf(p, "%15s %d %d %d",
                   ps[n].name, &ps[n].arrival, &ps[n].burst, &ps[n].priority) != 4)
            continue;
        n++;
    }
    fclose(fp);
    return n;
}

int main(int argc, char **argv) {
    const char *infile = NULL;
    const char *alg = "mlfq";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
            infile = argv[++i];
        else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc)
            alg = argv[++i];
        else if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s -f <case_file> [-a mlfq|cfs|all]\n", argv[0]);
            return 0;
        }
    }

    if (!infile) {
        printf("Use -f <file> to specify a case file.\n");
        printf("Format: name arrival burst priority (same as 01-scheduler)\n");
        return 1;
    }

    Process ps[MAX_PROCS];
    int n = load_from_file(infile, ps);
    if (n <= 0) { fprintf(stderr, "Failed to load %s\n", infile); return 1; }
    printf("Loaded %d processes from %s\n", n, infile);

    if (strcmp(alg, "mlfq") == 0 || strcmp(alg, "all") == 0)
        run_mlfq(ps, n);
    if (strcmp(alg, "cfs") == 0 || strcmp(alg, "all") == 0)
        run_cfs(ps, n);

    return 0;
}
