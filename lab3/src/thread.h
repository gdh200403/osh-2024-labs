#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>

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

ThreadPool *ThreadPool_Create(int num_threads, int queue_capacity);
int ThreadPool_Add(ThreadPool *pool, int task);
void ThreadPool_Destroy(ThreadPool *pool);

#endif // THREAD_H