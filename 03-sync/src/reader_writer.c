/*
 * reader_writer.c — 读者-写者问题
 *
 * 两种策略:
 *   rw rp — 读者优先 (reader-priority): 只要有读者在读，新读者可直接进入
 *   rw wp — 写者优先 (writer-priority): 有写者等待时新读者必须等待
 *
 * 用法: ./reader_writer [rp|wp] [num_readers] [num_writers] [iterations]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

/* 共享资源 */
static int shared_data = 0;
static int readers_active = 0;
static int writers_waiting = 0;

/* ---- 读者优先 (reader-priority) ---- */

typedef struct {
    pthread_mutex_t mutex;
    pthread_mutex_t rw_lock;   /* 写者或第一个读者持有 */
} RPState;

static RPState rp;

static void rp_init(RPState *s) {
    pthread_mutex_init(&s->mutex, NULL);
    pthread_mutex_init(&s->rw_lock, NULL);
}

static void *rp_reader(void *arg) {
    int id = *(int *)arg;
    int iters = *(((int *)arg) + 1);
    for (int i = 0; i < iters; i++) {
        pthread_mutex_lock(&rp.mutex);
        readers_active++;
        if (readers_active == 1)
            pthread_mutex_lock(&rp.rw_lock);  /* 第一个读者锁住写 */
        pthread_mutex_unlock(&rp.mutex);

        /* 读 */
        printf("[Reader-%d] read shared_data = %d (readers=%d)\n",
               id, shared_data, readers_active);
        usleep(rand() % 100000);

        pthread_mutex_lock(&rp.mutex);
        readers_active--;
        if (readers_active == 0)
            pthread_mutex_unlock(&rp.rw_lock); /* 最后一个读者释放写 */
        pthread_mutex_unlock(&rp.mutex);

        usleep(rand() % 200000);
    }
    return NULL;
}

static void *rp_writer(void *arg) {
    int id = *(int *)arg;
    int iters = *(((int *)arg) + 1);
    for (int i = 0; i < iters; i++) {
        usleep(rand() % 300000);  /* 思考 */

        pthread_mutex_lock(&rp.rw_lock);
        shared_data++;
        printf("[Writer-%d] wrote shared_data = %d\n", id, shared_data);
        usleep(rand() % 100000);
        pthread_mutex_unlock(&rp.rw_lock);
    }
    return NULL;
}

static void run_rp(int nreaders, int nwriters, int iters) {
    printf("\n===== Reader-Priority: %d readers, %d writers, %d iters =====\n",
           nreaders, nwriters, iters);
    rp_init(&rp);

    pthread_t *rtids = malloc(sizeof(pthread_t) * nreaders);
    pthread_t *wtids = malloc(sizeof(pthread_t) * nwriters);
    int *rargs = malloc(sizeof(int) * 2 * nreaders);
    int *wargs = malloc(sizeof(int) * 2 * nwriters);

    for (int i = 0; i < nreaders; i++) {
        rargs[i * 2] = i + 1;
        rargs[i * 2 + 1] = iters;
        pthread_create(&rtids[i], NULL, rp_reader, &rargs[i * 2]);
    }
    for (int i = 0; i < nwriters; i++) {
        wargs[i * 2] = i + 1;
        wargs[i * 2 + 1] = iters;
        pthread_create(&wtids[i], NULL, rp_writer, &wargs[i * 2]);
    }

    for (int i = 0; i < nreaders; i++) pthread_join(rtids[i], NULL);
    for (int i = 0; i < nwriters; i++) pthread_join(wtids[i], NULL);

    pthread_mutex_destroy(&rp.mutex);
    pthread_mutex_destroy(&rp.rw_lock);
    free(rtids); free(wtids); free(rargs); free(wargs);
}

/* ---- 写者优先 (writer-priority) ---- */

typedef struct {
    pthread_mutex_t mutex;
    pthread_mutex_t rw_lock;
    pthread_mutex_t write_mutex;  /* 写者优先的关键：写者排队锁 */
    int readers_active;
    int writers_waiting;
} WPState;

static WPState wp;

static void wp_init(WPState *s) {
    pthread_mutex_init(&s->mutex, NULL);
    pthread_mutex_init(&s->rw_lock, NULL);
    pthread_mutex_init(&s->write_mutex, NULL);
    s->readers_active = 0;
    s->writers_waiting = 0;
}

static void *wp_reader(void *arg) {
    int id = *(int *)arg;
    int iters = *(((int *)arg) + 1);
    for (int i = 0; i < iters; i++) {
        /* 先检查是否有写者在等 */
        pthread_mutex_lock(&wp.write_mutex);  /* 有写者等则在此阻塞 */
        pthread_mutex_lock(&wp.mutex);
        wp.readers_active++;
        if (wp.readers_active == 1)
            pthread_mutex_lock(&wp.rw_lock);
        pthread_mutex_unlock(&wp.mutex);
        pthread_mutex_unlock(&wp.write_mutex);

        printf("[Reader-%d] read shared_data = %d (readers=%d)\n",
               id, shared_data, wp.readers_active);
        usleep(rand() % 100000);

        pthread_mutex_lock(&wp.mutex);
        wp.readers_active--;
        if (wp.readers_active == 0)
            pthread_mutex_unlock(&wp.rw_lock);
        pthread_mutex_unlock(&wp.mutex);

        usleep(rand() % 200000);
    }
    return NULL;
}

static void *wp_writer(void *arg) {
    int id = *(int *)arg;
    int iters = *(((int *)arg) + 1);
    for (int i = 0; i < iters; i++) {
        usleep(rand() % 300000);

        pthread_mutex_lock(&wp.write_mutex);  /* 写者互斥排队 */
        pthread_mutex_lock(&wp.rw_lock);      /* 等所有读者离开 */
        shared_data++;
        printf("[Writer-%d] wrote shared_data = %d\n", id, shared_data);
        usleep(rand() % 100000);
        pthread_mutex_unlock(&wp.rw_lock);
        pthread_mutex_unlock(&wp.write_mutex);
    }
    return NULL;
}

static void run_wp(int nreaders, int nwriters, int iters) {
    printf("\n===== Writer-Priority: %d readers, %d writers, %d iters =====\n",
           nreaders, nwriters, iters);
    wp_init(&wp);

    pthread_t *rtids = malloc(sizeof(pthread_t) * nreaders);
    pthread_t *wtids = malloc(sizeof(pthread_t) * nwriters);
    int *rargs = malloc(sizeof(int) * 2 * nreaders);
    int *wargs = malloc(sizeof(int) * 2 * nwriters);

    for (int i = 0; i < nreaders; i++) {
        rargs[i * 2] = i + 1;
        rargs[i * 2 + 1] = iters;
        pthread_create(&rtids[i], NULL, wp_reader, &rargs[i * 2]);
    }
    for (int i = 0; i < nwriters; i++) {
        wargs[i * 2] = i + 1;
        wargs[i * 2 + 1] = iters;
        pthread_create(&wtids[i], NULL, wp_writer, &wargs[i * 2]);
    }

    for (int i = 0; i < nreaders; i++) pthread_join(rtids[i], NULL);
    for (int i = 0; i < nwriters; i++) pthread_join(wtids[i], NULL);

    pthread_mutex_destroy(&wp.mutex);
    pthread_mutex_destroy(&wp.rw_lock);
    pthread_mutex_destroy(&wp.write_mutex);
    free(rtids); free(wtids); free(rargs); free(wargs);
}

int main(int argc, char **argv) {
    const char *mode = "rp";
    int nreaders = 3, nwriters = 2, iters = 5;

    if (argc > 1) mode      = argv[1];
    if (argc > 2) nreaders  = atoi(argv[2]);
    if (argc > 3) nwriters  = atoi(argv[3]);
    if (argc > 4) iters     = atoi(argv[4]);

    srand((unsigned)time(NULL));

    if (strcmp(mode, "wp") == 0)
        run_wp(nreaders, nwriters, iters);
    else
        run_rp(nreaders, nwriters, iters);

    return 0;
}
