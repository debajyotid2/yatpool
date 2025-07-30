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

/****************************************************************************/
/******************************Task queue************************************/
/****************************************************************************/

#define EMPTY_QUEUE_VALUE 0

/// Task struct definition
typedef struct task {
    void *(*taskfunc)(void *);
    void *arg;
    void (*argdestructor)(void *);
} Task;

typedef struct queue {
    size_t capacity, start, end;
    Task **data;
} TaskQueue;

/// Initialize a TaskQueue
static inline TaskQueue* taskqueue_init(size_t capacity) {
    assert(capacity);

    TaskQueue* q = (TaskQueue *)malloc(sizeof(TaskQueue));
    if (!q) {
        ERR("taskqueue malloc failed.");
        return NULL;
    }

    q->data = (Task **)calloc(capacity+1, sizeof(Task *));
    if (!q->data) {
        ERR("taskqueue data calloc failed.");
        return NULL;
    }
    // A circular buffer needs 1 extra slot for differentiating
    // between full and empty
    q->capacity = capacity+1;
    q->start = q->end = 0;
    
    return q;
}

/// Add a value to the queue
static inline bool taskqueue_put(TaskQueue *q, Task *value) {
    if (q == NULL) {
        ERR("Null pointer for queue provided.");
        return false;
    }
    if (value == NULL) {
        ERR("Null pointer for value provided.");
        return false;
    }
    if ((q->end + 1) % q->capacity == q->start) {
        // Queue is full
        return false;
    }
    q->data[q->end] = value;
    q->end = (q->end + 1) % q->capacity;

    return true;
}

/// Check if the queue is empty
static inline bool taskqueue_empty(TaskQueue *q) {
    assert(q);
    return q->start == q->end;
}

/// Get the first element and remove it from the queue
static inline bool taskqueue_pop(TaskQueue *q, Task** task) {
    if (!(q && task)) {
        ERR("Null value for queue or task pointer provided.");
        return false;
    }
    if (taskqueue_empty(q)) {
        // Empty queue
        return false;
    }

    *task = q->data[q->start];
    q->start = (q->start + 1) % q->capacity;
    return true;
}

/// Check if the queue is full
static inline bool taskqueue_full(TaskQueue *q) {
    assert(q);
    return (q->end + 1) % q->capacity == q->start;
}

/// Destroy a TaskQueue instance
static inline void taskqueue_destroy(TaskQueue *q) {
    if (!q) {
        ERR("Null queue pointer.");
        return;
    }
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
    bool all_done;
    int tasks_completed, tasks_submitted;
    pthread_attr_t attr;
    pthread_mutex_t mutex;
    pthread_cond_t cond_queue, cond_slot_available, cond_curr_batch_done;
} YATPool;

/// Start a task thread
void *_yatpool_worker(void *arg) {
    YATPool *pool = (YATPool *)arg;
    bool all_done = false;
    Task *task;

    while (true) {

        pthread_mutex_lock(&pool->mutex);

        // Wait until a task is available in the queue
        while (taskqueue_empty(pool->task_queue) && !pool->all_done) {
            pthread_cond_wait(&pool->cond_queue, &pool->mutex);
        }
        if (pool->all_done && taskqueue_empty(pool->task_queue)) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }
        if (!taskqueue_pop(pool->task_queue, &task)) {
            pthread_mutex_unlock(&pool->mutex);
            continue;
        };
        pthread_cond_signal(&pool->cond_slot_available);
        pthread_mutex_unlock(&pool->mutex);

        // Execute task
        task->taskfunc(task->arg);
        if (task->argdestructor != NULL) {
            task->argdestructor(task->arg);
        }
        free(task);

        // Check if done
        pthread_mutex_lock(&pool->mutex);
        pool->tasks_completed++;
        if (pool->tasks_completed >= pool->tasks_submitted) {
            all_done = true;
        }
        pthread_mutex_unlock(&pool->mutex);
        if (all_done) {
            pthread_cond_broadcast(&pool->cond_curr_batch_done);
        }
    }
    return NULL;
}

/// Initialize a thread pool.
YATPool *yatpool_init(size_t num_threads, size_t queue_size) {
    if (num_threads == 0 || queue_size == 0) {
        ERR("num_threads and queue_size cannot be zero.");
        return NULL;
    }

    YATPool *pool = (YATPool *)malloc(sizeof(YATPool));
    if (!pool) {
        ERR("pool malloc failed");
        goto error;
    }

    pthread_attr_init(&pool->attr);
    pthread_cond_init(&pool->cond_queue, NULL);
    pthread_cond_init(&pool->cond_slot_available, NULL);
    pthread_cond_init(&pool->cond_curr_batch_done, NULL);
    pthread_mutex_init(&pool->mutex, NULL);

    pool->threads = (pthread_t *)calloc(num_threads, sizeof(pthread_t));
    if (!pool->threads) {
        ERR("threadpool calloc failed");
        goto error;
    }
    
    pool->task_queue = taskqueue_init(queue_size);
    if (!pool->task_queue) {
        ERR("taskqueue init failed");
        goto error;
    }

    pool->pool_size = num_threads;
    pool->all_done = false;
    pool->tasks_completed = 0;
    pool->tasks_submitted = 0;

    for (size_t i = 0; i < pool->pool_size; ++i) {
        if (pthread_create(&pool->threads[i], &pool->attr, &_yatpool_worker,
                           pool) != 0) {
            ERR("Could not create thread");
            pool->all_done = true;
            pthread_cond_broadcast(&pool->cond_queue);

            for (size_t j=0; j<i; ++j) {
                pthread_join(pool->threads[j], NULL);
            }
            goto error;
        }
    }
    return pool;

error:
    if (pool) {
        taskqueue_destroy(pool->task_queue);
        free(pool->threads);
        pthread_attr_destroy(&pool->attr);
        pthread_cond_destroy(&pool->cond_queue);
        pthread_cond_destroy(&pool->cond_slot_available);
        pthread_cond_destroy(&pool->cond_curr_batch_done);
        free(pool);
    }
    return NULL;
}

/// Submit a task to a threadpool
bool yatpool_put(YATPool *pool, void *(*taskfunc)(void *), void *arg,
                 void (*argdestructor)(void *)) {
    if (pool == NULL) {
        ERR("yatpool pointer is null.");
        return false;
    }
    if (taskfunc == NULL) {
        ERR("taskfunc cannot be null.");
        return false;
    }
    Task *task = (Task *)malloc(sizeof(Task));
    task->taskfunc = taskfunc;
    task->arg = arg;
    task->argdestructor = argdestructor;

    pthread_mutex_lock(&pool->mutex);
    if (pool->all_done) {
        pthread_mutex_unlock(&pool->mutex);
        free(task);
        return false;
    }

    // If the queue is full, wait
    while (taskqueue_full(pool->task_queue)) {
        pthread_cond_wait(&pool->cond_slot_available, &pool->mutex);
    }

    // Once queue has space, add task to queue
    taskqueue_put(pool->task_queue, task);
    pool->tasks_submitted++;
    pthread_cond_signal(&pool->cond_queue);
    pthread_mutex_unlock(&pool->mutex);

    return true;
}

/// Wait until all tasks are completed
bool yatpool_wait(YATPool *pool) {
    if (pool == NULL) {
        ERR("yatpool pointer is null.");
        return false;
    }

    pthread_mutex_lock(&pool->mutex);

    while (pool->tasks_completed < pool->tasks_submitted) {
        pthread_cond_wait(&pool->cond_curr_batch_done, &pool->mutex);
    }
    pool->tasks_completed = 0;
    pool->tasks_submitted = 0;

    pthread_mutex_unlock(&pool->mutex);
    return true;
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
bool yatpool_destroy(YATPool *pool) {
    if (pool == NULL) {
        ERR("yatpool pointer is null.");
        return false;
    }
    pthread_mutex_lock(&pool->mutex);
    if (pool->all_done) {
        pthread_mutex_unlock(&pool->mutex);
        return false;
    }
    pool->all_done = true;
    pthread_cond_broadcast(&pool->cond_queue);
    pthread_mutex_unlock(&pool->mutex);

    for (size_t i = 0; i < pool->pool_size; ++i) {
        if (pthread_join(pool->threads[i], NULL) != 0) {
            ERR("Failed to join threads.");
        }
    }
    
    // Free any remaining not done tasks
    while(!taskqueue_empty(pool->task_queue)) {
        Task *t;
        if(taskqueue_pop(pool->task_queue, &t)) {
            free(t);
        }
    }

    pthread_attr_destroy(&pool->attr);
    pthread_cond_destroy(&pool->cond_queue);
    pthread_cond_destroy(&pool->cond_slot_available);
    pthread_cond_destroy(&pool->cond_curr_batch_done);
    pthread_mutex_destroy(&pool->mutex);
    taskqueue_destroy(pool->task_queue);
    free(pool->threads);
    free(pool);
    return true;
}
