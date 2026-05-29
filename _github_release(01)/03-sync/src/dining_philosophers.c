/*
 * dining_philosophers.c — 哲学家就餐问题
 *
 * 死锁避免策略: 资源分级法 (resource hierarchy)
 *   编号最大的哲学家先拿编号大的筷子，其他人先拿编号小的。
 *   打破环形等待条件，保证无死锁。
 *
 * 用法: ./dining_philosophers [num_philosophers] [iterations]
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_PHIL 32

static pthread_mutex_t chopsticks[MAX_PHIL];
static int N;
static int iters;
static int eat_count[MAX_PHIL];

typedef struct {
    int id;
} PhilArg;

static void think(int id) {
    printf("[Philosopher %d] is thinking...\n", id);
    usleep(rand() % 300000);
}

static void eat(int id) {
    int left  = id;
    int right = (id + 1) % N;

    /* 资源分级：除最后一位外，都先拿编号小的 */
    if (id == N - 1) {
        /* 编号最大的哲学家先拿右筷 */
        pthread_mutex_lock(&chopsticks[right]);
        printf("[Philosopher %d] picked up right chopstick %d\n", id, right);
        pthread_mutex_lock(&chopsticks[left]);
        printf("[Philosopher %d] picked up left chopstick %d\n", id, left);
    } else {
        pthread_mutex_lock(&chopsticks[left]);
        printf("[Philosopher %d] picked up left chopstick %d\n", id, left);
        pthread_mutex_lock(&chopsticks[right]);
        printf("[Philosopher %d] picked up right chopstick %d\n", id, right);
    }

    eat_count[id]++;
    printf("[Philosopher %d] is EATING (meal #%d)\n", id, eat_count[id]);
    usleep(rand() % 200000);

    pthread_mutex_unlock(&chopsticks[left]);
    pthread_mutex_unlock(&chopsticks[right]);
    printf("[Philosopher %d] put down chopsticks\n", id);
}

static void *philosopher(void *arg) {
    int id = ((PhilArg *)arg)->id;
    for (int i = 0; i < iters; i++) {
        think(id);
        eat(id);
    }
    return NULL;
}

int main(int argc, char **argv) {
    N = 5;
    iters = 3;
    if (argc > 1) N     = atoi(argv[1]);
    if (argc > 2) iters = atoi(argv[2]);

    if (N < 2 || N > MAX_PHIL) N = 5;
    if (iters < 1) iters = 3;

    printf("===== Dining Philosophers (N=%d, iters=%d) =====\n", N, iters);
    printf("Strategy: resource hierarchy (philosopher %d takes right first)\n\n", N - 1);

    srand((unsigned)time(NULL));

    for (int i = 0; i < N; i++)
        pthread_mutex_init(&chopsticks[i], NULL);

    pthread_t tids[MAX_PHIL];
    PhilArg args[MAX_PHIL];
    for (int i = 0; i < N; i++) {
        args[i].id = i;
        pthread_create(&tids[i], NULL, philosopher, &args[i]);
    }

    for (int i = 0; i < N; i++)
        pthread_join(tids[i], NULL);

    printf("\n===== Meal Summary =====\n");
    int total = 0;
    for (int i = 0; i < N; i++) {
        printf("Philosopher %d: %d meals\n", i, eat_count[i]);
        total += eat_count[i];
    }
    printf("Total meals: %d (expected %d)\n", total, N * iters);

    for (int i = 0; i < N; i++)
        pthread_mutex_destroy(&chopsticks[i]);

    return 0;
}
