/* Numerical integration of area under y = 9 - x^2 between x = 0 and x = 3
 
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "yatpool.h"

#define MIN_ITS 1000000 /// Minimum number of iterations

typedef struct {
    double x_low, x_high, y_low, y_high;
    size_t num_its;
    size_t* hit_ctr_ptr;
} HitCtrArg;

void hitctrarg_init(HitCtrArg** arg, 
                    double x_low, 
                    double x_high, 
                    double y_low, 
                    double y_high, 
                    size_t num_its, 
                    size_t* hit_ctr_ptr) {
    if (arg==NULL || hit_ctr_ptr==NULL) return;
    *arg = (HitCtrArg*)malloc(sizeof(HitCtrArg));
    (*arg)->x_low = x_low;
    (*arg)->x_high = x_high;
    (*arg)->y_low = y_low;
    (*arg)->y_high = y_high;
    (*arg)->num_its = num_its;
    (*arg)->hit_ctr_ptr = hit_ctr_ptr;
}

void hitctrarg_destroy(void* arg) {
    if (arg==NULL) return;
    HitCtrArg* _arg = (HitCtrArg*)arg;
    free(_arg);
}

/// Function to be integrated
double func(double x) {
    return 9.0 - x * x;
}

/// Function that counts number of hits within range
void* count_hits(void* arg) {
    HitCtrArg* _arg = (HitCtrArg*)arg;

    size_t hits = 0;
    
    unsigned int seed = 42;
    for (size_t i=0; i<_arg->num_its; ++i) {
        double x = _arg->x_low + (double)rand_r(&seed)/(double)RAND_MAX * (_arg->x_high - _arg->x_low);
        double y = _arg->y_low + (double)rand_r(&seed)/(double)RAND_MAX * (_arg->y_high - _arg->y_low);
        if (y <= func(x))
            hits++;
        seed *= i;
    }
    *(_arg->hit_ctr_ptr) = hits;
    return NULL;
}

int main(int argc, char** argv) {
    size_t num_threads = 8;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <number of iterations>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    size_t num_its = atoi(argv[1]);
    if (num_its < MIN_ITS) {
        fprintf(stderr, "Must have at least %d iterations\n", MIN_ITS);
        return EXIT_FAILURE;
    }

    double x_low = 0.0, x_high = 3.0;
    double y_low = func(x_low);
    double y_high = func(x_high);

    size_t* hits = (size_t*)calloc(num_threads, sizeof(size_t));
    
    YATPool* pool;
    yatpool_init(&pool, num_threads, num_threads);

    for (size_t i=0; i<num_threads; ++i) {
        Task* task;
        HitCtrArg* arg;
        hitctrarg_init(&arg, x_low, x_high, y_low, y_high, num_its, &hits[i]);
        task_init(&task, &count_hits, arg, &hitctrarg_destroy);
        yatpool_put(pool, task);
    }

    yatpool_wait(pool);
    yatpool_destroy(pool);

    size_t total_hits = 0;
    for (size_t i=0; i<num_threads; ++i)
        total_hits += hits[i];

    printf("Numerically calculated = %f\n", (double)total_hits / (double)(num_its * num_threads) * (y_low-y_high) * (x_high-x_low));
    printf("Analytical solution = %f\n", 9.0 * (x_high - x_low) - (x_high * x_high * x_high - x_low * x_low * x_low) / 3.0);
    
    free(hits);

    return EXIT_SUCCESS;
}
