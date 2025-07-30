/* Generating data (random numbers) and writing to a csv file in parallel
 
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
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include "yatpool.h"

#define NCOLS 100       // Number of columns in each output file line
#define MAX_BUFLEN 10   // Maximum number of digits in each int to generate

typedef struct {
    char* data;
    size_t length;
} String;

typedef struct {
    size_t lineno;
    String* line;
} Line;

typedef struct {
    Line** lines;
    size_t start_lineno;
    size_t end_lineno;
} GenerateLinesArg;

typedef struct {
    Line** lines;
    size_t start_lineno;
    size_t end_lineno;
    size_t* offset_ptr;
} GetOffsetArg;

typedef struct {
    char* mapped_file;
    Line** lines;
    size_t start_lineno;
    size_t end_lineno;
    size_t offset;
} WriteToFileArg;

String string_create(const char* str, size_t length) {
    if (str == NULL) { 
        fprintf(stderr, "Null string provided.\n");
        abort();
    }
    String res;
    res.length = length;
    res.data = (char*)calloc(length+1, sizeof(char));
    strncpy(res.data, str, length);
    res.data[length] = '\0';
    return res;
}

void string_destroy(String* str) {
    if (str==NULL) return;
    free(str->data);
}

void string_print(const String* str) {
    if (str==NULL) return;
    printf("%s\n", str->data);
}

void line_init(Line** line, size_t lineno) {
    if (line==NULL) return;
    *line = (Line*)malloc(sizeof(Line));
    (*line)->line = (String*)malloc(sizeof(String));
    (*line)->lineno = lineno;
}

void line_destroy(Line* line) {
    if (line==NULL) return;
    string_destroy(line->line);
    free(line->line);
}

void generatelinesarg_init(GenerateLinesArg** arg, Line** lines, size_t start_lineno, size_t end_lineno) {
    if (arg==NULL || lines==NULL) return;
    GenerateLinesArg** _arg = (GenerateLinesArg**)arg;
    *_arg = (GenerateLinesArg*)malloc(sizeof(GenerateLinesArg));
    (*_arg)->lines = lines;
    (*_arg)->start_lineno = start_lineno;
    (*_arg)->end_lineno = end_lineno;
}

void generatelinesarg_destroy(void* arg) {
    if (arg==NULL) return;
    GenerateLinesArg* _arg = (GenerateLinesArg*)arg;
    free(_arg);
}

void getoffsetarg_init(GetOffsetArg** arg, 
                       Line** lines, 
                       size_t start_lineno,
                       size_t end_lineno,
                       size_t* offset_ptr) {
    if (arg==NULL) return;
    if (lines==NULL) return;
    *arg = (GetOffsetArg*)malloc(sizeof(GetOffsetArg));
    (*arg)->lines = lines;
    (*arg)->start_lineno = start_lineno;
    (*arg)->end_lineno = end_lineno;
    (*arg)->offset_ptr = offset_ptr;
}

void getoffsetarg_destroy(void* arg) {
    if (arg==NULL) return;
    GetOffsetArg* _arg = (GetOffsetArg*)arg;
    free(_arg);
}

void writetofilearg_init(WriteToFileArg** arg,
                         char* mapped,
                         Line** lines,
                         size_t start_lineno,
                         size_t end_lineno,
                         size_t offset) {
    if (arg==NULL || mapped==NULL || lines==NULL) return;
    *arg = (WriteToFileArg*)malloc(sizeof(WriteToFileArg));
    (*arg)->mapped_file = mapped;
    (*arg)->lines = lines;
    (*arg)->start_lineno = start_lineno;
    (*arg)->end_lineno = end_lineno;
    (*arg)->offset = offset;
}

void writetofilearg_destroy(void* arg) {
    if (arg==NULL) return;
    WriteToFileArg* _arg = (WriteToFileArg*)arg;
    free(_arg);
}

/// Function for threadpool to generate a bunch of lines of data
void* generate_lines(void* arg) {
    GenerateLinesArg* lnarg = (GenerateLinesArg*)arg;
    unsigned int seed = time(NULL) ^ lnarg->start_lineno;   // Unique seed
    
    // Create a line buffer to store a single line
    // Each number takes MAX_BUFLEN+1(comma)
    // Each line takes NCOLS*(MAX_BUFLEN+1)+2(\n + \0)
    size_t max_line_len = NCOLS*(MAX_BUFLEN+1)+2;
    char* line_buffer = (char*)calloc(max_line_len, sizeof(char));
    
    for (size_t i=lnarg->start_lineno; i<lnarg->end_lineno; ++i) {
        line_init(&(lnarg->lines[i]), i+1);

        char* ptr = line_buffer;
        size_t buflen = 0;
        for (int i=0; i<NCOLS; ++i) {
            int num_bytes_written = sprintf(ptr, "%d,", rand_r(&seed)%NCOLS);
            ptr += num_bytes_written;
            buflen += num_bytes_written;
        }
        line_buffer[buflen-1] = '\n';   // Last comma is replaced with a newline
        *(lnarg->lines[i]->line) = string_create(line_buffer, buflen);
    }

    free(line_buffer);
    return NULL;
}

/// Function for threadpool to calculate the offset required for a group of lines
void* get_offset(void* arg) {
    GetOffsetArg* offsetarg = (GetOffsetArg*)arg;
    *(offsetarg->offset_ptr) = 0;
    for (size_t i = offsetarg->start_lineno; i < offsetarg->end_lineno; ++i)
        *(offsetarg->offset_ptr) += offsetarg->lines[i]->line->length;
    return NULL;
}

/// Function for threadpool to write a chunk of lines into file
void* write_to_file(void* arg) {
    WriteToFileArg* currarg = (WriteToFileArg*)arg;

    size_t running_total_bytes = 0;
    currarg->mapped_file += currarg->offset;

    for (size_t i = currarg->start_lineno; i < currarg->end_lineno; ++i) {
        memcpy(currarg->mapped_file+running_total_bytes, currarg->lines[i]->line->data, currarg->lines[i]->line->length);
        running_total_bytes += currarg->lines[i]->line->length;
    }
    return NULL;
}

/// Comparison function for two Line objects in qsort
int cmp_lines(const void* a, const void* b) {
    Line* line_a = *(Line**)a;
    Line* line_b = *(Line**)b;
    return ((int)(line_a->lineno) - (int)(line_b->lineno));
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file to write to> <number of lines to write>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int num_lines = atoi(argv[2]);
    
    if (num_lines < 1) {
        fprintf(stderr, "Must specify at least one line to write.\n");
        return EXIT_FAILURE;
    }

    size_t num_threads = 8;

    struct timeval start, end;
    long duration = 0;  // microseconds
    
    gettimeofday(&start, NULL);

    size_t queue_size = num_threads * 8;

    YATPool* pool = yatpool_init(num_threads, queue_size);
   
    // Generate data in parallel
    Line** generated = (Line**)calloc(num_lines, sizeof(Line*));
    
    size_t fac = 8 * num_threads;
    size_t num_tasks = num_lines / fac;
    num_tasks = num_lines % fac == 0 ? num_tasks: num_tasks + 1;
        
    for(int i = 0; i < (int)num_tasks; ++i) {
        GenerateLinesArg* arg;

        size_t start_lineno = fac * i;
        size_t end_lineno = fac * (i + 1);
        end_lineno = end_lineno > (size_t)num_lines? num_lines: end_lineno;
        generatelinesarg_init(&arg, generated, start_lineno, end_lineno);
        
        yatpool_put(pool, &generate_lines, arg, &generatelinesarg_destroy);
    }
    yatpool_wait(pool);
    
    // Sort data so that it is in the correct order
    qsort(generated, num_lines, sizeof(Line*), cmp_lines);
    
    gettimeofday(&end, NULL);
    duration = (end.tv_sec-start.tv_sec)*1000000+\
                    (end.tv_usec-start.tv_usec);
    printf("Generating data took %g milliseconds.\n", (double)duration / 1000.0);

    gettimeofday(&start, NULL);

    num_tasks = num_lines / fac;
    num_tasks = num_lines % fac == 0 ? num_tasks: num_tasks + 1;
    
    // Calculate offsets for each group of lines to write
    size_t* offsets = (size_t*)calloc(num_tasks, sizeof(size_t));
    memset(offsets, 0, num_tasks * sizeof(size_t));

    for (size_t i = 0; i < num_tasks; ++i) {
        GetOffsetArg* arg;
        size_t start_lineno = fac * i;
        size_t end_lineno = fac * (i + 1);
        end_lineno = end_lineno > (size_t)num_lines? num_lines: end_lineno;
        getoffsetarg_init(&arg, generated, start_lineno, end_lineno, &offsets[i]);

        yatpool_put(pool, &get_offset, arg, &getoffsetarg_destroy);
    }

    yatpool_wait(pool);

    for (size_t i=1; i<num_tasks; ++i)
        offsets[i] += offsets[i-1];

    // Write data in parallel to file
    int fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd==-1) {
        fprintf(stderr, "Could not open file %s.\n", argv[1]);
        return EXIT_FAILURE;
    }

    // Set file size
    size_t file_size = offsets[num_tasks-1];
    if (ftruncate(fd, file_size)==-1) {
        fprintf(stderr, "Error truncating file to specified length.\n");
        close(fd);
        return EXIT_FAILURE;
    }

    char* file_buf = (char*)mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (file_buf == MAP_FAILED) {
        fprintf(stderr, "Error in mmap to file.\n");
        close(fd);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < num_tasks; ++i) {
        WriteToFileArg* arg;
        size_t start_lineno = fac * i;
        size_t end_lineno = fac * (i + 1);
        end_lineno = end_lineno > (size_t)num_lines? num_lines: end_lineno;
        size_t offset = (i == 0)? 0: offsets[i-1];
        writetofilearg_init(&arg, file_buf, generated, start_lineno, end_lineno, offset);

        yatpool_put(pool, &write_to_file, arg, &writetofilearg_destroy);
    }

    yatpool_wait(pool);
    yatpool_destroy(pool);

    munmap(file_buf, file_size);
    close(fd);

    gettimeofday(&end, NULL);
    duration = (end.tv_sec-start.tv_sec)*1000000+\
                    (end.tv_usec-start.tv_usec);
    printf("Writing data took %g milliseconds.\n", (double)duration / 1000.0);
    
    // Free all memory
    free(offsets);

    for (size_t i = 0; i < (size_t)num_lines; ++i) {
        line_destroy(generated[i]);
        free(generated[i]);
    }
    free(generated);

    return EXIT_SUCCESS;
}
