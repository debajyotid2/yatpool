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
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define NCOLS 100       // Number of columns in each output file line
#define MAX_BUFLEN 10   // Maximum number of digits in each int to generate

typedef struct {
    char* data;
    size_t length;
} String;

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

String generate_line() {
    char* joined = NULL;
    size_t buflen = 0;
    for (int i=0; i<NCOLS; ++i) {
        char num_str[MAX_BUFLEN];
        memset(num_str, '\0', sizeof(num_str));
        sprintf(num_str, "%d,", rand()%NCOLS);
        buflen += strlen(num_str);

        if (joined==NULL) {
            joined = (char*)calloc(buflen, sizeof(char));
            strncpy(joined, num_str, strlen(num_str));
        } else {
            joined = (char*)realloc(joined, buflen * sizeof(char));
            strncpy(&joined[buflen-strlen(num_str)], num_str, strlen(num_str));
        }
    }
    joined[buflen-1] = '\n';
    joined = (char*)realloc(joined, (buflen+1) * sizeof(char));
    joined[buflen] = '\0';

    String res = string_create(joined, buflen);
    free(joined);
    return res;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file to write to> <number of lines to write>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int num_lines = atoi(argv[2]);

    struct timeval start, end;
    long duration = 0;

    // Generate data
    gettimeofday(&start, NULL);

    size_t file_size = 0;
    String* lines = (String*)calloc(num_lines, sizeof(String));
    for (int i=0; i<num_lines; ++i) {
        lines[i] = generate_line();
        file_size += lines[i].length;
    }

    gettimeofday(&end, NULL);
    duration = (end.tv_sec-start.tv_sec)*1000000+\
                    (end.tv_usec-start.tv_usec);
    printf("Generating data took %g milliseconds.\n", (double)duration / 1000.0);
 
    // Write data to file
    gettimeofday(&start, NULL);
    
    int fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd==-1) {
        fprintf(stderr, "Could not open file %s.\n", argv[1]);
        return EXIT_FAILURE;
    }

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
 
    size_t running_total_bytes = 0;

    for (size_t i = 0; i < (size_t)num_lines; ++i) {
        memcpy(file_buf+running_total_bytes, lines[i].data, lines[i].length);
        running_total_bytes += lines[i].length;
        string_destroy(&lines[i]);
    }

    munmap(file_buf, file_size);

    gettimeofday(&end, NULL);
    duration = (end.tv_sec-start.tv_sec)*1000000+\
                    (end.tv_usec-start.tv_usec);
    printf("Writing data took %g milliseconds.\n", (double)duration / 1000.0);

    free(lines);
    
    return EXIT_SUCCESS;
}
