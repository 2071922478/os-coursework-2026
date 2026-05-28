/*
 * scheduler.c — 处理机调度算法模拟器
 *
 * 支持算法: FCFS / SJF(非抢占) / SRTF(SJF 抢占式) / RR / Priority
 * 输入   : 交互输入 或 -f <file> 批量
 * 输出   : ASCII 甘特图 + 每进程统计 + 平均等待/周转
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <getopt.h>

#define MAX_PROCS 64
#define MAX_NAME  16
#define MAX_TIME  4096

typedef struct {
    char  name[MAX_NAME];
    int   arrival;
    int   burst;
    int   priority;     /* 越小越优先 */
    /* runtime */
    int   remaining;
    int   start;        /* 第一次上 CPU 的时间，-1 = 未开始 */
    int   finish;
    bool  done;
} Process;

typedef enum { ALG_FCFS, ALG_SJF, ALG_SRTF, ALG_RR, ALG_PRIO } Alg;

/* --------- 工具 --------- */

static const char *alg_name(Alg a) {
    switch (a) {
        case ALG_FCFS: return "FCFS";
        case ALG_SJF:  return "SJF (non-preemptive)";
        case ALG_SRTF: return "SRTF (preemptive SJF)";
        case ALG_RR:   return "Round Robin";
        case ALG_PRIO: return "Priority (non-preemptive)";
    }
    return "?";
}

static void reset_runtime(Process *ps, int n) {
    for (int i = 0; i < n; ++i) {
        ps[i].remaining = ps[i].burst;
        ps[i].start = -1;
        ps[i].finish = 0;
        ps[i].done = false;
    }
}

/* 把一段时间 [t, t+len) 上的执行追加到甘特图 */
static void gantt_push(char *gantt_pid[MAX_TIME], int *gantt_len,
                       const char *who, int len) {
    for (int i = 0; i < len && *gantt_len < MAX_TIME; ++i) {
        gantt_pid[(*gantt_len)++] = strdup(who);
    }
}

static void print_gantt(char *gantt_pid[MAX_TIME], int total) {
    if (total == 0) { printf("(empty)\n"); return; }
    /* 把相邻相同名合并打印 */
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
    printf("\n");
    /* 时间刻度 */
    int t = 0;
    printf("0");
    i = 0;
    while (i < total) {
        int j = i;
        while (j < total && strcmp(gantt_pid[j], gantt_pid[i]) == 0) j++;
        int width = (j - i) * 2 + 2;
        t += (j - i);
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
    double sum_wait = 0, sum_turn = 0, sum_wtt = 0;
    printf("\n%-6s %-8s %-7s %-9s %-6s %-7s %-8s %-10s %-8s\n",
           "PID", "Arrival", "Burst", "Priority", "Start", "Finish",
           "Waiting", "Turnaround", "W-Turn");
    for (int i = 0; i < n; ++i) {
        int turn = ps[i].finish - ps[i].arrival;
        int wait = turn - ps[i].burst;
        double wtt = (double)turn / ps[i].burst;
        sum_wait += wait; sum_turn += turn; sum_wtt += wtt;
        printf("%-6s %-8d %-7d %-9d %-6d %-7d %-8d %-10d %-8.2f\n",
               ps[i].name, ps[i].arrival, ps[i].burst, ps[i].priority,
               ps[i].start, ps[i].finish, wait, turn, wtt);
    }
    printf("\nAverage waiting time   : %.2f\n", sum_wait / n);
    printf("Average turnaround time: %.2f\n", sum_turn / n);
    printf("Average W-turnaround   : %.2f\n", sum_wtt / n);
}

/* --------- 算法实现 --------- */

/* 选择函数：从已到达且未完成的进程里挑出 index，规则因算法而异 */
typedef int (*Picker)(Process *ps, int n, int t);

static int pick_fcfs(Process *ps, int n, int t) {
    int best = -1;
    for (int i = 0; i < n; ++i) {
        if (ps[i].done || ps[i].arrival > t) continue;
        if (best == -1 || ps[i].arrival < ps[best].arrival) best = i;
    }
    return best;
}

static int pick_sjf(Process *ps, int n, int t) {
    int best = -1;
    for (int i = 0; i < n; ++i) {
        if (ps[i].done || ps[i].arrival > t) continue;
        if (best == -1 || ps[i].burst < ps[best].burst
            || (ps[i].burst == ps[best].burst && ps[i].arrival < ps[best].arrival))
            best = i;
    }
    return best;
}

static int pick_prio(Process *ps, int n, int t) {
    int best = -1;
    for (int i = 0; i < n; ++i) {
        if (ps[i].done || ps[i].arrival > t) continue;
        if (best == -1 || ps[i].priority < ps[best].priority
            || (ps[i].priority == ps[best].priority && ps[i].arrival < ps[best].arrival))
            best = i;
    }
    return best;
}

/* 非抢占式调度的统一框架：FCFS / SJF / PRIO */
static void run_nonpreemptive(Process *ps, int n, Picker pick, const char *label) {
    reset_runtime(ps, n);
    char *gantt[MAX_TIME] = {0};
    int   glen = 0;
    int   t = 0, finished = 0;

    while (finished < n) {
        int idx = pick(ps, n, t);
        if (idx < 0) {
            /* CPU 空闲：跳到下一个到达时间 */
            int next = -1;
            for (int i = 0; i < n; ++i)
                if (!ps[i].done && (next == -1 || ps[i].arrival < next))
                    next = ps[i].arrival;
            if (next < 0) break;
            gantt_push(gantt, &glen, "idle", next - t);
            t = next;
            continue;
        }
        if (ps[idx].start < 0) ps[idx].start = t;
        gantt_push(gantt, &glen, ps[idx].name, ps[idx].burst);
        t += ps[idx].burst;
        ps[idx].finish = t;
        ps[idx].remaining = 0;
        ps[idx].done = true;
        finished++;
    }

    printf("\n===== %s =====\n", label);
    print_gantt(gantt, glen);
    print_stats(ps, n);
    free_gantt(gantt, glen);
}

/* SRTF：每个时间单位重新挑剩余时间最短 */
static void run_srtf(Process *ps, int n) {
    reset_runtime(ps, n);
    char *gantt[MAX_TIME] = {0};
    int   glen = 0;
    int   t = 0, finished = 0;

    while (finished < n) {
        int best = -1;
        for (int i = 0; i < n; ++i) {
            if (ps[i].done || ps[i].arrival > t) continue;
            if (best == -1 || ps[i].remaining < ps[best].remaining)
                best = i;
        }
        if (best < 0) {
            gantt_push(gantt, &glen, "idle", 1);
            t++;
            continue;
        }
        if (ps[best].start < 0) ps[best].start = t;
        gantt_push(gantt, &glen, ps[best].name, 1);
        ps[best].remaining--;
        t++;
        if (ps[best].remaining == 0) {
            ps[best].finish = t;
            ps[best].done = true;
            finished++;
        }
    }

    printf("\n===== SRTF (preemptive SJF) =====\n");
    print_gantt(gantt, glen);
    print_stats(ps, n);
    free_gantt(gantt, glen);
}

/* RR：FIFO 就绪队列 + 时间片 */
static void run_rr(Process *ps, int n, int quantum) {
    reset_runtime(ps, n);
    char *gantt[MAX_TIME] = {0};
    int   glen = 0;
    int   t = 0, finished = 0;

    /* 就绪队列（存索引） */
    int queue[MAX_PROCS * 64];
    int qhead = 0, qtail = 0;
    bool inq[MAX_PROCS] = {false};

    /* 按到达时间稳定排序的副本，用来扫描"刚到达的进程" */
    int order[MAX_PROCS];
    for (int i = 0; i < n; ++i) order[i] = i;
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (ps[order[j]].arrival < ps[order[i]].arrival) {
                int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }
    int scan = 0;
    /* 推入 t=0 时已到达的 */
    while (scan < n && ps[order[scan]].arrival <= t) {
        if (!inq[order[scan]]) { queue[qtail++] = order[scan]; inq[order[scan]] = true; }
        scan++;
    }

    while (finished < n) {
        if (qhead == qtail) {
            /* idle 一格 */
            gantt_push(gantt, &glen, "idle", 1);
            t++;
            while (scan < n && ps[order[scan]].arrival <= t) {
                if (!inq[order[scan]]) { queue[qtail++] = order[scan]; inq[order[scan]] = true; }
                scan++;
            }
            continue;
        }
        int idx = queue[qhead++];
        if (ps[idx].start < 0) ps[idx].start = t;
        int slice = ps[idx].remaining < quantum ? ps[idx].remaining : quantum;
        gantt_push(gantt, &glen, ps[idx].name, slice);
        ps[idx].remaining -= slice;
        t += slice;

        /* 在 slice 期间到达的，先入队（教材通用约定） */
        while (scan < n && ps[order[scan]].arrival <= t) {
            if (!inq[order[scan]]) { queue[qtail++] = order[scan]; inq[order[scan]] = true; }
            scan++;
        }

        if (ps[idx].remaining == 0) {
            ps[idx].finish = t;
            ps[idx].done = true;
            finished++;
        } else {
            /* 未完成的回队尾 */
            queue[qtail++] = idx;
        }
    }

    printf("\n===== RR (q = %d) =====\n", quantum);
    print_gantt(gantt, glen);
    print_stats(ps, n);
    free_gantt(gantt, glen);
}

/* --------- 输入 --------- */

static int load_from_file(const char *path, Process *ps) {
    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return -1; }
    char line[256];
    int n = 0;
    while (fgets(line, sizeof line, fp)) {
        /* 跳过空行与注释 */
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '#') continue;
        if (n >= MAX_PROCS) { fprintf(stderr, "too many processes\n"); break; }
        if (sscanf(p, "%15s %d %d %d",
                   ps[n].name, &ps[n].arrival, &ps[n].burst, &ps[n].priority) != 4) {
            fprintf(stderr, "bad line: %s", line);
            continue;
        }
        n++;
    }
    fclose(fp);
    return n;
}

static int load_from_stdin(Process *ps) {
    int n;
    printf("Number of processes (<=%d): ", MAX_PROCS);
    if (scanf("%d", &n) != 1 || n <= 0 || n > MAX_PROCS) return -1;
    printf("Enter for each: name arrival burst priority\n");
    for (int i = 0; i < n; ++i) {
        printf("  P%d > ", i + 1);
        if (scanf("%15s %d %d %d", ps[i].name,
                  &ps[i].arrival, &ps[i].burst, &ps[i].priority) != 4)
            return -1;
    }
    return n;
}

/* --------- main --------- */

static void usage(const char *prog) {
    printf("Usage: %s [options]\n"
           "  -f <file>       load processes from file\n"
           "  -a <alg>        fcfs | sjf | srtf | rr | prio | all\n"
           "  -q <quantum>    RR time slice (default 2)\n"
           "  -h              this help\n"
           "\nWith no -f, run interactive menu.\n", prog);
}

static void run_one(Alg alg, Process *ps, int n, int q) {
    /* 工作副本，避免污染 */
    Process buf[MAX_PROCS];
    memcpy(buf, ps, sizeof(Process) * n);
    switch (alg) {
        case ALG_FCFS: run_nonpreemptive(buf, n, pick_fcfs, "FCFS"); break;
        case ALG_SJF:  run_nonpreemptive(buf, n, pick_sjf,  "SJF (non-preemptive)"); break;
        case ALG_PRIO: run_nonpreemptive(buf, n, pick_prio, "Priority (non-preemptive)"); break;
        case ALG_SRTF: run_srtf(buf, n); break;
        case ALG_RR:   run_rr(buf, n, q); break;
    }
}

static void interactive_menu(Process *ps, int n, int q) {
    while (1) {
        printf("\n================ Menu ================\n");
        printf("1) FCFS\n2) SJF (non-preemptive)\n3) SRTF (preemptive)\n");
        printf("4) Round Robin (q=%d, change with 7)\n5) Priority (non-preemptive)\n", q);
        printf("6) Compare ALL\n7) Set RR quantum\n0) Quit\n");
        printf("Choose: ");
        int c;
        if (scanf("%d", &c) != 1) break;
        switch (c) {
            case 1: run_one(ALG_FCFS, ps, n, q); break;
            case 2: run_one(ALG_SJF,  ps, n, q); break;
            case 3: run_one(ALG_SRTF, ps, n, q); break;
            case 4: run_one(ALG_RR,   ps, n, q); break;
            case 5: run_one(ALG_PRIO, ps, n, q); break;
            case 6:
                run_one(ALG_FCFS, ps, n, q);
                run_one(ALG_SJF,  ps, n, q);
                run_one(ALG_SRTF, ps, n, q);
                run_one(ALG_RR,   ps, n, q);
                run_one(ALG_PRIO, ps, n, q);
                break;
            case 7:
                printf("New quantum: "); if (scanf("%d", &q) != 1 || q <= 0) q = 2;
                break;
            case 0: return;
            default: printf("invalid\n");
        }
    }
}

int main(int argc, char **argv) {
    const char *infile = NULL;
    const char *algstr = NULL;
    int quantum = 2;
    int opt;
    while ((opt = getopt(argc, argv, "f:a:q:h")) != -1) {
        switch (opt) {
            case 'f': infile = optarg; break;
            case 'a': algstr = optarg; break;
            case 'q': quantum = atoi(optarg); if (quantum <= 0) quantum = 2; break;
            case 'h': default: usage(argv[0]); return 0;
        }
    }

    Process ps[MAX_PROCS];
    int n;
    if (infile) {
        n = load_from_file(infile, ps);
        if (n <= 0) return 1;
        printf("Loaded %d processes from %s\n", n, infile);
    } else {
        n = load_from_stdin(ps);
        if (n <= 0) { fprintf(stderr, "input error\n"); return 1; }
    }

    if (algstr) {
        if      (!strcmp(algstr, "fcfs")) run_one(ALG_FCFS, ps, n, quantum);
        else if (!strcmp(algstr, "sjf"))  run_one(ALG_SJF,  ps, n, quantum);
        else if (!strcmp(algstr, "srtf")) run_one(ALG_SRTF, ps, n, quantum);
        else if (!strcmp(algstr, "rr"))   run_one(ALG_RR,   ps, n, quantum);
        else if (!strcmp(algstr, "prio")) run_one(ALG_PRIO, ps, n, quantum);
        else if (!strcmp(algstr, "all")) {
            run_one(ALG_FCFS, ps, n, quantum);
            run_one(ALG_SJF,  ps, n, quantum);
            run_one(ALG_SRTF, ps, n, quantum);
            run_one(ALG_RR,   ps, n, quantum);
            run_one(ALG_PRIO, ps, n, quantum);
        } else { fprintf(stderr, "unknown alg: %s\n", algstr); return 1; }
        (void)alg_name;
    } else {
        interactive_menu(ps, n, quantum);
    }
    return 0;
}
