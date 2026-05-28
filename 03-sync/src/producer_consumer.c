/*
 * producer_consumer.c — 多生产者-多消费者 (有界缓冲)
 *
 * 使用 POSIX semaphore:
 *   empty  — 缓冲空位数 (初值 = BUFFER_SIZE)
 *   full   — 缓冲已填充数 (初值 = 0)
 *   mutex  — 互斥访问缓冲
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define MAX_BUF 128

typedef struct {
    int  buf[MAX_BUF];
    int  in, out;
    int  size;
    sem_t empty, full, mutex;
    int  produced_total;
    int  consumed_total;
} Buffer;

static Buffer g_buf;

static void buf_init(Buffer *b, int size) {
    b->size = size;
    b->in = b->out = 0;
    b->produced_total = b->consumed_total = 0;
    sem_init(&b->empty, 0, size);
    sem_init(&b->full,  0, 0);
    sem_init(&b->mutex, 0, 1);
}

static void buf_destroy(Buffer *b) {
    sem_destroy(&b->empty);
    sem_destroy(&b->full);
    sem_destroy(&b->mutex);
}

typedef struct {
    int id;
    int items;    /* 每个生产者生产的总数 */
} ProducerArg;

typedef struct {
    int id;
    int items;    /* 每个消费者消费的总数 */
} ConsumerArg;

static void *producer(void *arg) {
    ProducerArg *pa = (ProducerArg *)arg;
    for (int i = 0; i < pa->items; i++) {
        int item = (pa->id << 16) | i;  /* 唯一产品号 */
        sem_wait(&g_buf.empty);
        sem_wait(&g_buf.mutex);

        g_buf.buf[g_buf.in] = item;
        g_buf.in = (g_buf.in + 1) % g_buf.size;
        g_buf.produced_total++;
        printf("[P%d] produced item #%d (total produced: %d, buf: %d/%d)\n",
               pa->id, i, g_buf.produced_total,
               (g_buf.in - g_buf.out + g_buf.size) % g_buf.size,
               g_buf.size);

        sem_post(&g_buf.mutex);
        sem_post(&g_buf.full);
        usleep(rand() % 200000);  /* 模拟生产耗时 */
    }
    return NULL;
}

static void *consumer(void *arg) {
    ConsumerArg *ca = (ConsumerArg *)arg;
    for (int i = 0; i < ca->items; i++) {
        sem_wait(&g_buf.full);
        sem_wait(&g_buf.mutex);

        int item = g_buf.buf[g_buf.out];
        g_buf.out = (g_buf.out + 1) % g_buf.size;
        g_buf.consumed_total++;
        printf("[C%d] consumed item 0x%08x (total consumed: %d, buf: %d/%d)\n",
               ca->id, item, g_buf.consumed_total,
               (g_buf.in - g_buf.out + g_buf.size) % g_buf.size,
               g_buf.size);

        sem_post(&g_buf.mutex);
        sem_post(&g_buf.empty);
        usleep(rand() % 300000);  /* 模拟消费耗时 */
    }
    return NULL;
}

int main(int argc, char **argv) {
    int nproducers = 2, nconsumers = 2, buf_size = 10, items_each = 15;

    if (argc > 1) nproducers = atoi(argv[1]);
    if (argc > 2) nconsumers = atoi(argv[2]);
    if (argc > 3) buf_size    = atoi(argv[3]);
    if (argc > 4) items_each  = atoi(argv[4]);

    if (buf_size < 1 || buf_size > MAX_BUF) buf_size = 10;
    if (items_each < 1) items_each = 10;

    printf("===== Producer-Consumer =====\n");
    printf("Producers: %d, Consumers: %d, Buffer: %d, Items each: %d\n",
           nproducers, nconsumers, buf_size, items_each);

    srand((unsigned)time(NULL));
    buf_init(&g_buf, buf_size);

    pthread_t *ptids = malloc(sizeof(pthread_t) * nproducers);
    pthread_t *ctids = malloc(sizeof(pthread_t) * nconsumers);
    ProducerArg *pargs = malloc(sizeof(ProducerArg) * nproducers);
    ConsumerArg *cargs = malloc(sizeof(ConsumerArg) * nconsumers);

    for (int i = 0; i < nproducers; i++) {
        pargs[i].id = i + 1;
        pargs[i].items = items_each;
        pthread_create(&ptids[i], NULL, producer, &pargs[i]);
    }
    for (int i = 0; i < nconsumers; i++) {
        cargs[i].id = i + 1;
        cargs[i].items = (nproducers * items_each) / nconsumers;
        if (i == nconsumers - 1)
            cargs[i].items += (nproducers * items_each) % nconsumers; /* 余数 */
        pthread_create(&ctids[i], NULL, consumer, &cargs[i]);
    }

    for (int i = 0; i < nproducers; i++) pthread_join(ptids[i], NULL);
    for (int i = 0; i < nconsumers; i++) pthread_join(ctids[i], NULL);

    printf("\nDone. Total produced: %d, Total consumed: %d\n",
           g_buf.produced_total, g_buf.consumed_total);

    free(ptids); free(ctids); free(pargs); free(cargs);
    buf_destroy(&g_buf);
    return 0;
}
