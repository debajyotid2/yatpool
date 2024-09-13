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

#include <assert.h>
#include "yatpool.h"

#define ERR(msg) fprintf(stderr, "%s, line %d: Error: %s\n", __FILE__, __LINE__, msg);
#define ERR_AND_EXIT(msg) {\
    fprintf(stderr, "%s, line %d: Error: %s\n", __FILE__, __LINE__, msg); \
    exit(1); \
}

/// Size of the task queue of a threadpool
#define MAX_QUEUE_SIZE 100

/****************************************************************************/
/******************************Task queue************************************/
/****************************************************************************/

#define EMPTY_QUEUE_VALUE 0

typedef struct queue {
    size_t length, curr_size;
    void** data;
} TaskQueue;

/// Initialize a TaskQueue
void taskqueue_init(TaskQueue** q, size_t length) {
    assert(length);

    *q = (TaskQueue*)malloc(sizeof(TaskQueue));
    
    (*q)->data = (void**)calloc(length, sizeof(void *));
    (*q)->length = length;
    (*q)->curr_size = 0;
}

/// Add a value to the queue
bool taskqueue_put(TaskQueue *q, void* value) {
    if (q == NULL) {
        ERR("Null pointer for queue provided.");
        return false;
    }
    if (value == NULL) {
        ERR("Null pointer for value provided.");
        return false;
    }
    if ((q->curr_size + 1) > q->length) {
        return false;
    }
    q->data[q->curr_size] = value;
    (q->curr_size)++;

    return true;
}

/// Get the current size of the queue
size_t taskqueue_size(TaskQueue *q) {
    if (q == NULL) ERR_AND_EXIT("Null value for queue pointer provided.");
    return q->curr_size;
}

/// Get the first element of the queue
void* taskqueue_get(TaskQueue *q) {
    if (q == NULL) ERR_AND_EXIT("Null value for queue pointer provided.");
    if (q->curr_size == 0) {
        return EMPTY_QUEUE_VALUE;
    }
    return q->data[0];
}


/// Get the first element and remove it from the queue
void* taskqueue_pop(TaskQueue *q) {
    if (q == NULL) ERR_AND_EXIT("Null value for queue pointer provided.");
    if (q->curr_size == 0) {
        return EMPTY_QUEUE_VALUE;
    }

    void* elem = q->data[0];
    for (size_t i = 0; i < q->curr_size - 1; i++) {
        q->data[i] = q->data[i + 1];
    }
    q->curr_size--;
    return elem;
}

/// Check if the queue is empty
bool taskqueue_empty(TaskQueue *q) {
    if (q == NULL) ERR_AND_EXIT("Null value for queue pointer provided.");
    return q->curr_size == 0;
}

/// Check if the queue is full
bool taskqueue_full(TaskQueue *q) {
    if (q == NULL) ERR_AND_EXIT("Null value for queue pointer provided.");
    return (q->curr_size) == (q->length);
}

/// Clear the task queue
void taskqueue_clear(TaskQueue *q) {
    if (q == NULL) ERR_AND_EXIT("Null value for queue pointer provided.");
    for (size_t i=0; i<q->curr_size; ++i)
        free(q->data[i]);
    q->curr_size = 0;
}

/// Destroy a TaskQueue instance
void taskqueue_destroy(TaskQueue *q) {
    if (q == NULL) ERR_AND_EXIT("Null value for queue pointer provided.");
    
    for (size_t i=0; i<q->curr_size; ++i)
        free(q->data[i]);
    free(q->data);
    free(q);
}

/****************************************************************************/
/******************************Thread pool***********************************/
/****************************************************************************/

/// Threadpool struct definition
typedef struct yatpool {
    pthread_t* threads;
    size_t pool_size;
    TaskQueue* task_queue;
    void** retvalarr;
    bool done;
    int completed, total_tasks;
    pthread_attr_t attr;
    pthread_mutex_t mutex;
    pthread_cond_t cond_queue, cond_slot_available, cond_done;
} YATPool;

/// Task struct definition
typedef struct task {
    void* (*taskfunc)(void *);
    void* arg;
    void (*argdestructor)(void *);
} Task;

/// Initialize a Task object
void task_init(Task **task, void *(*taskfunc)(void *), void *arg, void (*argdestructor)(void *)) {
    if (task==NULL) {
        ERR("Task pointer is null.");
        return;
    }
    if (taskfunc==NULL) {
        ERR("taskfunc cannot be null.");
        return;
    }
    *task = (Task*)malloc(sizeof(Task));
    (*task)->taskfunc = taskfunc;
    (*task)->arg = arg;
    (*task)->argdestructor = argdestructor;
    return;
}

/// Execute a task
void* _yatpool_execute(YATPool* pool, Task* task) {
    if (task==NULL) {
        ERR("task pointer is null.");
        return NULL;
    }
    if (pool==NULL) {
        ERR("yatpool pointer is null.");
        return NULL;
    }

    // Execute task
    void* result = task->taskfunc(task->arg);

    // Check if done
    pthread_mutex_lock(&pool->mutex);
    pool->retvalarr[pool->completed] = result;
    pool->completed++;
    if (pool->completed>=pool->total_tasks) {
        pool->done = true;
        pthread_cond_broadcast(&pool->cond_done);
    }
    pthread_mutex_unlock(&pool->mutex);

    // Destroy task
    if (task->argdestructor!=NULL)
        task->argdestructor(task->arg);
    free(task);

    return result;
}

/// Start a task thread
void* _yatpool_start_thread(void* arg) {
    YATPool* pool = (YATPool*)arg;

    while (true) {
        Task* task;

        pthread_mutex_lock(&pool->mutex);

        // Wait until a slot is available in the queue
        while (taskqueue_empty(pool->task_queue) && !pool->done) {
            pthread_cond_wait(&pool->cond_queue, &pool->mutex);
        }
        if (pool->done) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }
        task = (Task *)taskqueue_pop(pool->task_queue);

        if (taskqueue_empty(pool->task_queue)) {
            pthread_cond_signal(&pool->cond_slot_available);
        }
        pthread_mutex_unlock(&pool->mutex);
        _yatpool_execute(pool, task);
    }
    return NULL;
}

/// Create threads
void _yatpool_create_threads(YATPool* pool) {
    if (pool==NULL) {
        ERR("yatpool pointer is null.");
        return;
    }

    for (size_t i = 0; i < pool->pool_size; ++i) {
        if (pthread_create(&pool->threads[i], &pool->attr, &_yatpool_start_thread, pool) != 0)
            ERR_AND_EXIT("Could not create thread");
    }
}

/// Initialize a thread pool.
void yatpool_init(YATPool** pool, size_t num_threads, size_t num_tasks) {
    if (num_threads==0) ERR_AND_EXIT("num_threads cannot be zero.");
    if (num_tasks==0) ERR_AND_EXIT("num_tasks cannot be zero.");

    if (pool==NULL) {
        ERR("yatpool pointer is null.");
        return;
    }

    *pool = (YATPool*)malloc(sizeof(YATPool));

    (*pool)->threads = (pthread_t*)calloc(num_threads, sizeof(YATPool));
    
    taskqueue_init(&(*pool)->task_queue, MAX_QUEUE_SIZE);

    (*pool)->retvalarr = (void**)calloc(num_tasks, sizeof(void*));
    
    pthread_attr_init(&(*pool)->attr);
    pthread_cond_init(&(*pool)->cond_queue, NULL);
    pthread_cond_init(&(*pool)->cond_slot_available, NULL);
    pthread_cond_init(&(*pool)->cond_done, NULL);
    pthread_mutex_init(&(*pool)->mutex, NULL);

    (*pool)->total_tasks = num_tasks;
    (*pool)->pool_size = num_threads;
    (*pool)->done = false;
    (*pool)->completed = 0;

    _yatpool_create_threads(*pool);
};

/// Submit a task to a threadpool
void yatpool_put(YATPool* pool, Task* task) {
    if (task==NULL) {
        ERR("task pointer is null.");
        return;
    }
    if (pool==NULL) {
        ERR("yatpool pointer is null.");
        return;
    }

    pthread_mutex_lock(&pool->mutex);
    
    // If the queue is full, wait
    while (taskqueue_full(pool->task_queue)) {
        pthread_cond_wait(&pool->cond_slot_available, &pool->mutex);
    }

    // Once queue has space, add task to queue
    taskqueue_put(pool->task_queue, (void *)task);
    pthread_mutex_unlock(&pool->mutex);
    pthread_cond_signal(&pool->cond_queue);
    
    return;
}

/// Wait until all tasks are completed
void** yatpool_wait(YATPool* pool) {
    if (pool==NULL) {
        ERR("yatpool pointer is null.");
        return NULL;
    }

    pthread_mutex_lock(&pool->mutex);

    while (!pool->done) {
        pthread_cond_wait(&pool->cond_done, &pool->mutex);
    }
    pthread_cond_broadcast(&pool->cond_queue);
    pthread_mutex_unlock(&pool->mutex);

    for (size_t i = 0; i < pool->pool_size; ++i) {
        if (pthread_join(pool->threads[i], NULL) != 0) 
            ERR_AND_EXIT("Failed to join threads.");
    }
    return pool->retvalarr;
}

/// Join all threads in a thread pool.
void** yatpool_join(YATPool* pool) {
    if (pool==NULL) {
        ERR("yatpool pointer is null.");
        return NULL;
    }
    for (size_t i = 0; i < pool->pool_size; ++i) {
        if (pthread_join(pool->threads[i], NULL) != 0) 
            ERR_AND_EXIT("Failed to join threads.");
    }
    return pool->retvalarr;
}

/// Reset a thread pool without joining threads or destroying it.
void yatpool_reset(YATPool* pool, size_t num_tasks) {
    if (pool==NULL) {
        ERR("yatpool pointer is null.");
        return;
    }
    if (num_tasks==0) ERR_AND_EXIT("num_tasks cannot be zero.");
    if (!(pool->done))
        ERR_AND_EXIT("Previous task pool not completed. Reset failed.");

    taskqueue_clear(pool->task_queue);
    pool->done = false;
    pool->total_tasks = num_tasks;
    pool->completed = 0;
}

/// Get the number of threads in a thread pool
size_t yatpool_pool_size(YATPool* pool) {
    if (pool==NULL) {
        ERR("yatpool pointer is null.");
        return 0;
    }
    return pool->pool_size;
}

/// Destroy a thread pool.
void yatpool_destroy(YATPool* pool) {
    if (pool==NULL) {
        ERR("yatpool pointer is null.");
        return;
    }
    pthread_attr_destroy(&pool->attr);
    pthread_cond_destroy(&pool->cond_queue);
    pthread_cond_destroy(&pool->cond_slot_available);
    pthread_cond_destroy(&pool->cond_done);
    pthread_mutex_destroy(&pool->mutex);
    taskqueue_destroy(pool->task_queue);
    free(pool->threads);

    for (int i=0; i<pool->total_tasks; ++i)
        free(pool->retvalarr[i]);
    free(pool->retvalarr);
    free(pool);
    return;
};
