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

#ifndef YATPOOL_H
#define YATPOOL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

typedef struct yatpool YATPool;
typedef struct task Task;

void task_init(Task** task, void*(*taskfunc)(void *), void* arg, void(*argdestructor)(void *));
void yatpool_init(YATPool** pool, size_t num_threads, size_t num_tasks);
void** yatpool_wait(YATPool* pool);
void yatpool_put(YATPool* pool, Task* task);
size_t yatpool_pool_size(YATPool* pool);
void yatpool_destroy(YATPool* pool);

#endif // _YATPOOL_H_
