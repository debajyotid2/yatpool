/*

    YATPool - Yet Another Thread Pool implemented in C

    Copyright (C) 2024  Debajyoti Debnath

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

 */

#include "yatpool.h"
#include <assert.h>

#define ERR(msg)                                                               \
    fprintf(stderr, "%s, line %d: Error: %s\n", __FILE__, __LINE__, msg);
#define ERR_AND_EXIT(msg)                                                      \
    {                                                                          \
        fprintf(stderr, "%s, line %d: Error: %s\n", __FILE__, __LINE__, msg);  \
        exit(1);                                                               \
    }

/// Size of the task queue of a threadpool
#define MAX_QUEUE_SIZE 256

/****************************************************************************/
/******************************Task queue************************************/
/****************************************************************************/

#define EMPTY_QUEUE_VALUE 0

typedef struct queue {
    size_t length, curr_size, start, end;
    void **data;
} TaskQueue;

/// Initialize a TaskQueue
static inline void taskqueue_init(TaskQueue **q, size_t length) {
    assert(length);

    *q = (TaskQueue *)malloc(sizeof(TaskQueue));

    (*q)->data = (void **)calloc(length, sizeof(void *));
    (*q)->length = length;
    (*q)->curr_size = 0;
    (*q)->start = (*q)->end = 0;
}

/// Add a value to the queue
static inline bool taskqueue_put(TaskQueue *q, void *value) {
    if (q == NULL) {
        ERR("Null pointer for queue provided.");
        return false;
    }
    if (value == NULL) {
        ERR("Null pointer for value provided.");
        return false;
    }
    if ((q->end + 1) % q->length == q->start) {
        // Queue is full
        return false;
    }
    q->data[q->end] = value;
    q->end = (q->end + 1) % q->length;
    (q->curr_size)++;

    return true;
}

/// Get the first element of the queue, without removing it
static inline void *taskqueue_get(TaskQueue *q) {
    if (q == NULL)
        ERR_AND_EXIT("Null value for queue pointer provided.");
    if (q->start == q->end) {
        // Empty queue
        return EMPTY_QUEUE_VALUE;
    }
    return q->data[q->start];
}

/// Get the first element and remove it from the queue
static inline void *taskqueue_pop(TaskQueue *q) {
    if (q == NULL)
        ERR_AND_EXIT("Null value for queue pointer provided.");
    if (q->start == q->end) {
        // Empty queue
        return EMPTY_QUEUE_VALUE;
    }

    void *elem = q->data[q->start];
    q->start = (q->start + 1) % q->length;
    q->curr_size--;
    return elem;
}

/// Check if the queue is empty
static inline bool taskqueue_empty(TaskQueue *q) {
    if (q == NULL)
        ERR_AND_EXIT("Null value for queue pointer provided.");
    return q->start == q->end;
}

/// Check if the queue is full
static inline bool taskqueue_full(TaskQueue *q) {
    if (q == NULL)
        ERR_AND_EXIT("Null value for queue pointer provided.");
    return (q->end + 1) % q->length == q->start;
}

/// Destroy a TaskQueue instance
static inline void taskqueue_destroy(TaskQueue *q) {
    if (q == NULL)
        ERR_AND_EXIT("Null value for queue pointer provided.");

    free(q->data);
    free(q);
}

/****************************************************************************/
/******************************Thread pool***********************************/
/****************************************************************************/

/// Threadpool struct definition
typedef struct yatpool {
    pthread_t *threads;
    size_t pool_size;
    TaskQueue *task_queue;
    bool done;
    int tasks_completed, tasks_submitted;
    pthread_attr_t attr;
    pthread_mutex_t mutex;
    pthread_cond_t cond_queue, cond_slot_available, cond_done;
} YATPool;

/// Task struct definition
typedef struct task {
    void *(*taskfunc)(void *);
    void *arg;
    void (*argdestructor)(void *);
} Task;

/// Start a task thread
void *_yatpool_worker(void *arg) {
    YATPool *pool = (YATPool *)arg;

    while (true) {
        Task *task;

        pthread_mutex_lock(&pool->mutex);

        // Wait until a task is available in the queue
        while (taskqueue_empty(pool->task_queue) && !pool->done) {
            pthread_cond_wait(&pool->cond_queue, &pool->mutex);
        }
        if (pool->done) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }
        task = (Task *)taskqueue_pop(pool->task_queue);
        pthread_cond_signal(&pool->cond_slot_available);
        pthread_mutex_unlock(&pool->mutex);

        // Execute task
        task->taskfunc(task->arg);
        // Destroy task
        if (task->argdestructor != NULL) {
            task->argdestructor(task->arg);
        }
        free(task);

        // Check if done
        pthread_mutex_lock(&pool->mutex);
        pool->tasks_completed++;
        if (pool->tasks_completed >= pool->tasks_submitted) {
            pthread_cond_broadcast(&pool->cond_done);
        }
        pthread_mutex_unlock(&pool->mutex);
    }
    return NULL;
}

/// Initialize a thread pool.
void yatpool_init(YATPool **pool_ptr, size_t num_threads) {
    if (num_threads == 0)
        ERR_AND_EXIT("num_threads cannot be zero.");

    if (pool_ptr == NULL) {
        ERR("yatpool pointer is null.");
        return;
    }

    *pool_ptr = (YATPool *)malloc(sizeof(YATPool));
    YATPool *pool = *pool_ptr;

    pool->threads = (pthread_t *)calloc(num_threads, sizeof(pthread_t));

    taskqueue_init(&pool->task_queue, MAX_QUEUE_SIZE);

    pthread_attr_init(&pool->attr);
    pthread_cond_init(&pool->cond_queue, NULL);
    pthread_cond_init(&pool->cond_slot_available, NULL);
    pthread_cond_init(&pool->cond_done, NULL);
    pthread_mutex_init(&pool->mutex, NULL);

    pool->pool_size = num_threads;
    pool->done = false;
    pool->tasks_completed = 0;
    pool->tasks_submitted = 0;

    for (size_t i = 0; i < pool->pool_size; ++i) {
        if (pthread_create(&pool->threads[i], &pool->attr, &_yatpool_worker,
                           pool) != 0) {
            ERR_AND_EXIT("Could not create thread");
        }
    }
};

/// Submit a task to a threadpool
void yatpool_put(YATPool *pool, void *(*taskfunc)(void *), void *arg,
                 void (*argdestructor)(void *)) {
    if (pool == NULL) {
        ERR("yatpool pointer is null.");
        return;
    }
    if (taskfunc == NULL) {
        ERR("taskfunc cannot be null.");
        return;
    }
    Task *task = (Task *)malloc(sizeof(Task));
    task->taskfunc = taskfunc;
    task->arg = arg;
    task->argdestructor = argdestructor;

    pthread_mutex_lock(&pool->mutex);

    // If the queue is full, wait
    while (taskqueue_full(pool->task_queue)) {
        pthread_cond_wait(&pool->cond_slot_available, &pool->mutex);
    }

    // Once queue has space, add task to queue
    taskqueue_put(pool->task_queue, (void *)task);
    pool->tasks_submitted++;
    pthread_cond_signal(&pool->cond_queue);
    pthread_mutex_unlock(&pool->mutex);

    return;
}

/// Wait until all tasks are completed
void yatpool_wait(YATPool *pool) {
    if (pool == NULL) {
        ERR("yatpool pointer is null.");
        return;
    }

    pthread_mutex_lock(&pool->mutex);

    while (pool->tasks_completed < pool->tasks_submitted) {
        pthread_cond_wait(&pool->cond_done, &pool->mutex);
    }
    pool->done = false;
    pool->tasks_completed = 0;
    pool->tasks_submitted = 0;

    pthread_mutex_unlock(&pool->mutex);
    return;
}

/// Get the number of threads in a thread pool
size_t yatpool_pool_size(YATPool *pool) {
    if (pool == NULL) {
        ERR("yatpool pointer is null.");
        return 0;
    }
    return pool->pool_size;
}

/// Destroy a thread pool.
void yatpool_destroy(YATPool *pool) {
    if (pool == NULL) {
        ERR("yatpool pointer is null.");
        return;
    }
    pool->done = true;
    pthread_cond_broadcast(&pool->cond_queue);
    pthread_mutex_unlock(&pool->mutex);

    for (size_t i = 0; i < pool->pool_size; ++i) {
        if (pthread_join(pool->threads[i], NULL) != 0)
            ERR_AND_EXIT("Failed to join threads.");
    }

    pthread_attr_destroy(&pool->attr);
    pthread_cond_destroy(&pool->cond_queue);
    pthread_cond_destroy(&pool->cond_slot_available);
    pthread_cond_destroy(&pool->cond_done);
    pthread_mutex_destroy(&pool->mutex);
    taskqueue_destroy(pool->task_queue);
    free(pool->threads);
    free(pool);
    return;
}
