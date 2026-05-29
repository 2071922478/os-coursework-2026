/*
 * realtime_demo.c — 实时调度策略演示
 *
 * 使用 sched_setscheduler 设置 SCHED_FIFO / SCHED_RR 策略，
 * 对比不同优先级线程的响应时间。
 *
 * 编译: gcc -O2 -Wall -pthread -o realtime_demo realtime_demo.c
 * 运行: sudo ./realtime_demo          (需要 root 权限设置实时调度)
 *       sudo ./realtime_demo fifo 3   (SCHED_FIFO, 3 个优先级)
 *       sudo ./realtime_demo rr 4     (SCHED_RR, 4 个优先级)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

typedef struct {
    int id;
    int policy;       /* SCHED_FIFO or SCHED_RR */
    int priority;     /* 1..99, 越高越优先 */
    int iterations;
} ThreadArg;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static const char *policy_name(int policy) {
    switch (policy) {
        case SCHED_FIFO: return "SCHED_FIFO";
        case SCHED_RR:   return "SCHED_RR";
        case SCHED_OTHER:return "SCHED_OTHER";
    }
    return "?";
}

static void *worker(void *arg) {
    ThreadArg *ta = (ThreadArg *)arg;
    uint64_t total_wait = 0, max_wait = 0;

    for (int i = 0; i < ta->iterations; i++) {
        uint64_t start = now_ns();

        /* 模拟 CPU 密集型工作 (自旋) */
        volatile uint64_t work = 0;
        for (volatile int w = 0; w < 5000000; w++) work++;

        uint64_t end = now_ns();
        uint64_t elapsed = end - start;
        total_wait += elapsed;
        if (elapsed > max_wait) max_wait = elapsed;

        if (i % 10 == 0 || i == ta->iterations - 1) {
            printf("[Thread %d] %s prio=%2d iter=%3d  elapsed=%8llu us\n",
                   ta->id, policy_name(ta->policy), ta->priority,
                   i, (unsigned long long)elapsed / 1000);
        }
    }

    printf("[Thread %d] %s prio=%2d DONE. avg=%llu us  max=%llu us\n",
           ta->id, policy_name(ta->policy), ta->priority,
           (unsigned long long)(total_wait / ta->iterations) / 1000,
           (unsigned long long)max_wait / 1000);

    return NULL;
}

int main(int argc, char **argv) {
    const char *mode = "fifo";
    int num_threads = 4;
    int iters = 30;

    if (argc > 1) mode = argv[1];
    if (argc > 2) num_threads = atoi(argv[2]);
    if (argc > 3) iters = atoi(argv[3]);

    int policy;
    if (strcmp(mode, "rr") == 0)
        policy = SCHED_RR;
    else if (strcmp(mode, "other") == 0)
        policy = SCHED_OTHER;
    else
        policy = SCHED_FIFO;

    printf("===== Real-time Scheduling Demo =====\n");
    printf("Policy: %s, Threads: %d, Iterations: %d\n\n",
           policy_name(policy), num_threads, iters);

    pthread_t tids[32];
    ThreadArg args[32];
    pthread_attr_t attrs[32];
    struct sched_param sched_params[32];

    for (int i = 0; i < num_threads; i++) {
        args[i].id = i;
        args[i].policy = policy;
        args[i].priority = (policy != SCHED_OTHER) ? (num_threads - i + 10) : 0;
        args[i].iterations = iters;

        pthread_attr_init(&attrs[i]);
        pthread_attr_setinheritsched(&attrs[i], PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attrs[i], policy);

        if (policy != SCHED_OTHER) {
            sched_params[i].sched_priority = args[i].priority;
            pthread_attr_setschedparam(&attrs[i], &sched_params[i]);
        }

        pthread_create(&tids[i], &attrs[i], worker, &args[i]);
        pthread_attr_destroy(&attrs[i]);
    }

    for (int i = 0; i < num_threads; i++)
        pthread_join(tids[i], NULL);

    printf("\nAll threads finished.\n");
    return 0;
}
