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

#define MIN_ITS 1000000 /// Minimum number of iterations

/// Function to be integrated
double func(double x) {
    return 9.0 - x * x;
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

    num_its *= num_threads;

    double x_low = 0.0, x_high = 3.0;
    double y_low = func(x_low);
    double y_high = func(x_high);

    size_t hits = 0;
    
    for (size_t i=0; i<num_its; ++i) {
        double x = x_low + (double)rand()/(double)RAND_MAX * (x_high - x_low);
        double y = y_low + (double)rand()/(double)RAND_MAX * (y_high - y_low);
        if (y <= func(x))
            hits++;
    }

    printf("Numerically calculated = %f\n", (double)hits / (double)num_its * (y_low-y_high) * (x_high-x_low));
    printf("Analytical solution = %f\n", 9.0 * (x_high - x_low) - (x_high * x_high * x_high - x_low * x_low * x_low) / 3.0);

    return EXIT_SUCCESS;
}
