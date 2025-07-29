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
    size_t length, curr_size, start, end;
    void** data;
} TaskQueue;

/// Initialize a TaskQueue
void taskqueue_init(TaskQueue** q, size_t length) {
    assert(length);

    *q = (TaskQueue*)malloc(sizeof(TaskQueue));
    
    (*q)->data = (void**)calloc(length, sizeof(void *));
    (*q)->length = length;
    (*q)->curr_size = 0;
    (*q)->start = (*q)->end = 0;
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
    if ((q->end + 1) % q->length == q->start) {
        // Queue is full
        return false;
    }
    q->data[q->end] = value;
    q->end = (q->end + 1) % q->length;
    (q->curr_size)++;

    return true;
}

/// Get the current size of the queue
size_t taskqueue_size(TaskQueue *q) {
    if (q == NULL) ERR_AND_EXIT("Null value for queue pointer provided.");
    return q->curr_size;
}

/// Get the first element of the queue, without removing it
void* taskqueue_get(TaskQueue *q) {
    if (q == NULL) ERR_AND_EXIT("Null value for queue pointer provided.");
    if (q->start == q->end) {
        // Empty queue
        return EMPTY_QUEUE_VALUE;
    }
    return q->data[q->start];
}


/// Get the first element and remove it from the queue
void* taskqueue_pop(TaskQueue *q) {
    if (q == NULL) ERR_AND_EXIT("Null value for queue pointer provided.");
    if (q->start == q->end) {
        // Empty queue
        return EMPTY_QUEUE_VALUE;
    }

    void* elem = q->data[q->start];
    q->start = (q->start + 1) % q->length;
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
    return (q->end + 1) % q->length == q->start;
}

/// Clear the task queue
void taskqueue_clear(TaskQueue *q) {
    if (q == NULL) ERR_AND_EXIT("Null value for queue pointer provided.");
    q->curr_size = 0;
    if (q->start == q->end) {
        // Empty queue
        return;
    }
    for (size_t i=q->start; i!=q->end; i=(i+1) % q->length) {
        free(q->data[i]);
    }
}

/// Destroy a TaskQueue instance
void taskqueue_destroy(TaskQueue *q) {
    if (q == NULL) ERR_AND_EXIT("Null value for queue pointer provided.");
    
    taskqueue_clear(q);
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
    bool active;
    pthread_attr_t attr;
    pthread_mutex_t mutex;
    pthread_cond_t cond_queue, cond_slot_available;
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


/// Start a task thread
void* _yatpool_start_thread(void* arg) {
    YATPool* pool = (YATPool*)arg;
    Task* task;

    while (true) {
        pthread_mutex_lock(&pool->mutex);

        // Wait until a task is available in the queue
        while (taskqueue_empty(pool->task_queue) && pool->active) {
            pthread_cond_wait(&pool->cond_queue, &pool->mutex);
        }
        if (!pool->active && taskqueue_empty(pool->task_queue)) {
            pthread_mutex_unlock(&pool->mutex);
            return NULL;
        }
        task = (Task *)taskqueue_pop(pool->task_queue);
        pthread_cond_signal(&pool->cond_slot_available);
        pthread_mutex_unlock(&pool->mutex);
        
        // Execute task
        if (task) {
            void* result = task->taskfunc(task->arg);
            // Destroy task
            if (task->argdestructor) {
                task->argdestructor(task->arg);
            }
            free(task);
        }
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
        if (pthread_create(&pool->threads[i], &pool->attr, &_yatpool_start_thread, pool) != 0) {
            ERR_AND_EXIT("Could not create thread");
        }
    }
}

/// Initialize a thread pool.
void yatpool_init(YATPool** pool, size_t num_threads) {
    if (num_threads==0) ERR_AND_EXIT("num_threads cannot be zero.");

    if (pool==NULL) {
        ERR("yatpool pointer is null.");
        return;
    }

    *pool = (YATPool*)malloc(sizeof(YATPool));

    (*pool)->threads = (pthread_t*)calloc(num_threads, sizeof(pthread_t));
   
    taskqueue_init(&(*pool)->task_queue, MAX_QUEUE_SIZE);

    pthread_attr_init(&(*pool)->attr);
    pthread_cond_init(&(*pool)->cond_queue, NULL);
    pthread_cond_init(&(*pool)->cond_slot_available, NULL);
    pthread_mutex_init(&(*pool)->mutex, NULL);

    (*pool)->pool_size = num_threads;
    (*pool)->active = true;

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

    if (!pool->active) {
        ERR("cannot put tasks when pool is inactive.");
        pthread_mutex_unlock(&pool->mutex);
        return;
    }
    
    // If the queue is full, wait
    while (taskqueue_full(pool->task_queue)) {
        pthread_cond_wait(&pool->cond_slot_available, &pool->mutex);
    }

    // Once queue has space, add task to queue
    taskqueue_put(pool->task_queue, (void *)task);
    pthread_cond_signal(&pool->cond_queue);
    pthread_mutex_unlock(&pool->mutex);
    
    return;
}

/// Destroy a thread pool.
void yatpool_destroy(YATPool* pool) {
    if (pool==NULL) {
        ERR("yatpool pointer is null.");
        return;
    }
    pthread_mutex_lock(&pool->mutex);
    pool->active = false;
    pthread_cond_broadcast(&pool->cond_queue);
    pthread_mutex_unlock(&pool->mutex);

    for (size_t i = 0; i < pool->pool_size; ++i) {
        if (pthread_join(pool->threads[i], NULL) != 0) 
            ERR_AND_EXIT("Failed to join threads.");
    }
    pthread_attr_destroy(&pool->attr);
    pthread_cond_destroy(&pool->cond_queue);
    pthread_cond_destroy(&pool->cond_slot_available);
    pthread_mutex_destroy(&pool->mutex);
    taskqueue_destroy(pool->task_queue);
    free(pool->threads);
    free(pool);
    return;
}
