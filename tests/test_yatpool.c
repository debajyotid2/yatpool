/* 
    Tests for YATPool

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

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include "yatpool.h"

// Test Globals & Thread-Safe Helper Functions
#define NUM_THREADS 4
#define QUEUE_SIZE 64

static pthread_mutex_t g_test_mutex;
static int g_task_counter;
static bool g_destructor_called;

void* simple_increment_task(void *arg) {
    (void)arg;
    pthread_mutex_lock(&g_test_mutex);
    g_task_counter++;
    pthread_mutex_unlock(&g_test_mutex);
    return NULL;
}

void* slow_increment_task(void *arg) {
    (void)arg;
    usleep(10000); // 10ms
    pthread_mutex_lock(&g_test_mutex);
    g_task_counter++;
    pthread_mutex_unlock(&g_test_mutex);
    return NULL;
}

void* destructor_task(void *arg) {
    (void)arg;
    return NULL;
}

void arg_destructor_func(void *arg) {
    (void)arg;
    pthread_mutex_lock(&g_test_mutex);
    g_destructor_called = true;
    pthread_mutex_unlock(&g_test_mutex);
}

// Test Cases
TEST_CASE("Pool Lifecycle and Configuration", "[lifecycle]") {
    SECTION("Creation with invalid arguments") {
        YATPool *p1 = yatpool_init(0, 10);
        REQUIRE(p1 == NULL);

        YATPool *p2 = yatpool_init(4, 0);
        REQUIRE(p2 == NULL);
    }

    SECTION("Get pool size") {
        YATPool *pool;
        pool = yatpool_init(NUM_THREADS, QUEUE_SIZE);
        REQUIRE(pool != NULL);

        REQUIRE(yatpool_pool_size(pool) == NUM_THREADS);
        REQUIRE(yatpool_pool_size(NULL) == 0);
        yatpool_destroy(pool);
    }
}

TEST_CASE("Task Execution", "[tasks]") {
    SECTION("A single task is executed") {
        YATPool *pool;
        pool = yatpool_init(NUM_THREADS, QUEUE_SIZE);
        REQUIRE(pool != NULL);
        
        pthread_mutex_init(&g_test_mutex, NULL);

        pthread_mutex_lock(&g_test_mutex);
        g_task_counter = 0;
        g_destructor_called = false;
        pthread_mutex_unlock(&g_test_mutex);
        
        REQUIRE(yatpool_put(pool, simple_increment_task, NULL, NULL) == true);
        yatpool_wait(pool);

        pthread_mutex_lock(&g_test_mutex);
        REQUIRE(g_task_counter == 1);
        pthread_mutex_unlock(&g_test_mutex);
        
        yatpool_destroy(pool);
        pthread_mutex_destroy(&g_test_mutex);
    }

    SECTION("More tasks than threads are all completed") {
        YATPool *pool;
        pool = yatpool_init(NUM_THREADS, QUEUE_SIZE);
        REQUIRE(pool != NULL);
        
        pthread_mutex_init(&g_test_mutex, NULL);

        pthread_mutex_lock(&g_test_mutex);
        g_task_counter = 0;
        g_destructor_called = false;
        pthread_mutex_unlock(&g_test_mutex);
 
        const int num_tasks = 20;
        for (int i = 0; i < num_tasks; ++i) {
            REQUIRE(yatpool_put(pool, simple_increment_task, NULL, NULL) == true);
        }
        yatpool_wait(pool);

        pthread_mutex_lock(&g_test_mutex);
        REQUIRE(g_task_counter == num_tasks);
        pthread_mutex_unlock(&g_test_mutex);

        yatpool_destroy(pool);
        pthread_mutex_destroy(&g_test_mutex);
    }

    SECTION("Pool can be reused after waiting") {
        YATPool *pool;
        pool = yatpool_init(NUM_THREADS, QUEUE_SIZE);
        REQUIRE(pool != NULL);
        
        pthread_mutex_init(&g_test_mutex, NULL);

        pthread_mutex_lock(&g_test_mutex);
        g_task_counter = 0;
        g_destructor_called = false;
        pthread_mutex_unlock(&g_test_mutex);
 
        const int batch1_tasks = 10;
        for (int i = 0; i < batch1_tasks; ++i) {
            yatpool_put(pool, simple_increment_task, NULL, NULL);
        }
        yatpool_wait(pool);
        
        pthread_mutex_lock(&g_test_mutex);
        REQUIRE(g_task_counter == batch1_tasks);
        g_task_counter = 0; // Reset for next batch
        pthread_mutex_unlock(&g_test_mutex);

        const int batch2_tasks = 15;
        for (int i = 0; i < batch2_tasks; ++i) {
            yatpool_put(pool, slow_increment_task, NULL, NULL);
        }
        yatpool_wait(pool);

        pthread_mutex_lock(&g_test_mutex);
        REQUIRE(g_task_counter == batch2_tasks);
        pthread_mutex_unlock(&g_test_mutex);

        yatpool_destroy(pool);
        pthread_mutex_destroy(&g_test_mutex);
    }
}

TEST_CASE("Task Arguments and Destruction", "[args]") {
    SECTION("Argument destructor is called") {
        YATPool *pool;
        pool = yatpool_init(NUM_THREADS, QUEUE_SIZE);
        REQUIRE(pool != NULL);
        
        pthread_mutex_init(&g_test_mutex, NULL);

        pthread_mutex_lock(&g_test_mutex);
        g_task_counter = 0;
        g_destructor_called = false;
        pthread_mutex_unlock(&g_test_mutex);
 
        int* dummy_arg = (int*)malloc(sizeof(int));
        REQUIRE(dummy_arg != NULL);
        *dummy_arg = 5;

        yatpool_put(pool, destructor_task, dummy_arg, arg_destructor_func);
        yatpool_wait(pool);

        pthread_mutex_lock(&g_test_mutex);
        REQUIRE(g_destructor_called == true);
        pthread_mutex_unlock(&g_test_mutex);
        
        free(dummy_arg);
        
        yatpool_destroy(pool);
        pthread_mutex_destroy(&g_test_mutex);
    }
}

TEST_CASE("Pool Shutdown", "[shutdown]") {
    pthread_mutex_init(&g_test_mutex, NULL);
    pthread_mutex_lock(&g_test_mutex);
    g_task_counter = 0;
    pthread_mutex_unlock(&g_test_mutex);
    
    YATPool* pool = yatpool_init(NUM_THREADS, QUEUE_SIZE);
    REQUIRE(pool != NULL);

    const int num_tasks = 50;
    for (int i = 0; i < num_tasks; ++i) {
        yatpool_put(pool, slow_increment_task, NULL, NULL);
    }

    yatpool_destroy(pool);

    // We can check the final state of the global counter.
    pthread_mutex_lock(&g_test_mutex);
    REQUIRE(g_task_counter == num_tasks);
    pthread_mutex_unlock(&g_test_mutex);
    
    pthread_mutex_destroy(&g_test_mutex);
}
