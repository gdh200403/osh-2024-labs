#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "server.h"

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
    pthread_t *threads;
    int *tasks;
    int thread_count, task_count, queue_capacity;
    int head, tail;
    int stop;
} ThreadPool;

static void *worker(void *arg);

ThreadPool *ThreadPool_Create(int num_threads, int queue_capacity) {
    ThreadPool *pool = (ThreadPool *)malloc(sizeof(ThreadPool));
    if (!pool) return NULL;

    pool->thread_count = num_threads;
    pool->queue_capacity = queue_capacity;
    pool->task_count = 0;
    pool->head = pool->tail = 0;
    pool->stop = 0;

    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
    pool->tasks = (int *)malloc(sizeof(int) * queue_capacity);

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->not_full, NULL);
    pthread_cond_init(&pool->not_empty, NULL);

    for (int i = 0; i < num_threads; ++i) {
        pthread_create(&pool->threads[i], NULL, worker, pool);
    }

    return pool;
}

static void *worker(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;

    while (1) {
        pthread_mutex_lock(&pool->lock);

        while (pool->task_count == 0 && !pool->stop) {
            pthread_cond_wait(&pool->not_empty, &pool->lock);
        }

        if (pool->stop) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        int task = pool->tasks[pool->head];
        pool->head = (pool->head + 1) % pool->queue_capacity;
        --pool->task_count;

        pthread_cond_signal(&pool->not_full);
        pthread_mutex_unlock(&pool->lock);

        // Process task
        handle_clnt(task);
    }

    return NULL;
}

int ThreadPool_Add(ThreadPool *pool, int task) {
    pthread_mutex_lock(&pool->lock);

    while (pool->task_count == pool->queue_capacity && !pool->stop) {
        pthread_cond_wait(&pool->not_full, &pool->lock);
    }

    if (pool->stop) {
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }

    pool->tasks[pool->tail] = task;
    pool->tail = (pool->tail + 1) % pool->queue_capacity;
    ++pool->task_count;

    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->lock);
    return 0;
}

void ThreadPool_Destroy(ThreadPool *pool) {
    pthread_mutex_lock(&pool->lock);
    pool->stop = 1;
    pthread_cond_broadcast(&pool->not_empty);
    pthread_mutex_unlock(&pool->lock);

    for (int i = 0; i < pool->thread_count; ++i) {
        pthread_join(pool->threads[i], NULL);
    }

    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->not_full);
    pthread_cond_destroy(&pool->not_empty);
    free(pool->tasks);
    free(pool->threads);
    free(pool);
}