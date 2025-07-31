/* Multithreaded image blurring
 
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "yatpool.h"

#define NUM_THREADS 8
#define IMAGE_WIDTH 2048
#define IMAGE_HEIGHT 2048
#define BLUR_KERNEL_SIZE 21 // Must be an odd number

#define NUM_WORMS 200
#define WORM_LENGTH 1000
#define WORM_RADIUS 20

// Struct to hold RGB pixel data
typedef struct {
    unsigned char r, g, b;
} Pixel;

// Struct to represent an image
typedef struct {
    int width, height;
    Pixel *data;
} Image;

// Argument struct for blur tasks
typedef struct {
    const Image *src_image;
    Image *dest_image;
    int start_row, end_row;
} BlurTaskArg;

// Argument struct for Perlin worm generation
typedef struct {
    Image *image;
    size_t start_worm_no, end_worm_no;
} WormTaskArg;

// Creates a blank image of a given size.
Image *create_image(int width, int height) {
    Image *img = (Image *)malloc(sizeof(Image));
    if (!img)
        return NULL;
    img->width = width;
    img->height = height;
    img->data = (Pixel *)calloc(width * height, sizeof(Pixel));
    if (!img->data) {
        free(img);
        return NULL;
    }
    return img;
}

// Frees the memory associated with an image.
void destroy_image(Image *img) {
    if (img) {
        free(img->data);
        free(img);
    }
}

// Perlin noise implementation (https://mrl.cs.nyu.edu/~perlin/paper445.pdf)
// (from original author in Java: https://cs.nyu.edu/~perlin/noise/)
static const int p[] = {
    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,
    225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190,
    6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117,
    35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136,
    171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158,
    231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,
    245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,  209,
    76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130, 116, 188, 159, 86,
    164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124, 123, 5,
    202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,  16,
    58,  17,  182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,
    154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,  253,
    19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104, 218, 246, 97,
    228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,  51,
    145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157, 184,
    84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,
    222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156,
    180, 151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233,
    7,   225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,
    190, 6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203,
    117, 35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125,
    136, 171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146,
    158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,
    46,  245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,
    209, 76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130, 116, 188, 159,
    86,  164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124, 123,
    5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,
    16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,
    44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,
    253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104, 218, 246,
    97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,
    51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157,
    184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205,
    93,  222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,
    156, 180};

static double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static double lerp(double t, double a, double b) { return a + t * (b - a); }
// From https://github.com/stegu/perlin-noise/blob/master/src/noise1234.c, grad2
static double grad(int hash, double x, double y) {
    int h = hash & 7;         // Convert low 3 bits of hash code
    double u = h < 4 ? x : y; // into 8 simple gradient directions
    double v = h < 4 ? y : x; // and compute the dot product with (x, y)
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

static double perlin(double x, double y) {
    int xi = (int)floor(x) & 255;
    int yi = (int)floor(y) & 255;
    double xf = x - floor(x);
    double yf = y - floor(y);

    double u = fade(xf);
    double v = fade(yf);

    int aa = p[p[xi] + yi];
    int ab = p[p[xi] + yi + 1];
    int ba = p[p[xi + 1] + yi];
    int bb = p[p[xi + 1] + yi + 1];

    double x1 = lerp(u, grad(aa, xf, yf), grad(ba, xf - 1, yf));
    double x2 = lerp(u, grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1));

    return lerp(v, x1, x2);
}

// Generate test images with black perlin worms on white background
void *generate_worms(void *arg) {
    WormTaskArg *task_arg = (WormTaskArg *)arg;
    Image *img = task_arg->image;
    unsigned int seed = time(NULL) ^ task_arg->start_worm_no;

    for (int i = task_arg->start_worm_no; i < task_arg->end_worm_no; ++i) {
        // Start position for worm
        double x = rand_r(&seed) % img->width;
        double y = rand_r(&seed) % img->height;

        // For smoother paths use different random numbers
        double nx = (double)rand_r(&seed) / RAND_MAX * 1000.0;
        double ny = (double)rand_r(&seed) / RAND_MAX * 1000.0;

        for (int j = 0; j < WORM_LENGTH; ++j) {
            // Get direction from Perlin noise
            double angle = perlin(nx, ny) * 2.0 * M_PI;

            // Move the worm
            x += cos(angle);
            y += sin(angle);
            nx += 0.02;
            ny += 0.02;

            // Draw a circle at the worm's current position
            int ix = (int)x;
            int iy = (int)y;
            for (int cy = -WORM_RADIUS; cy <= WORM_RADIUS; ++cy) {
                for (int cx = -WORM_RADIUS; cx <= WORM_RADIUS; ++cx) {
                    if (cx * cx + cy * cy <= WORM_RADIUS * WORM_RADIUS) {
                        int draw_x = ix + cx;
                        int draw_y = iy + cy;
                        // Check bounds before drawing
                        if (draw_x >= 0 && draw_x < img->width && draw_y >= 0 &&
                            draw_y < img->height) {
                            img->data[draw_y * img->width + draw_x] =
                                (Pixel){0, 0, 0};
                        }
                    }
                }
            }

            // Wrap around edges
            if (x < 0)
                x = img->width - 1;
            if (x >= img->width)
                x = 0;
            if (y < 0)
                y = img->height - 1;
            if (y >= img->height)
                y = 0;
        }
    }
    return NULL;
}

// Destructor for WormTaskArg
void destroy_worm_task_arg(void *arg) {
    WormTaskArg *_arg = (WormTaskArg *)arg;
    if (_arg) {
        free(_arg);
    }
}

// Saves an image to a file in the simple PPM P3 format.
// (https://users.csc.calpoly.edu/~akeen/courses/csc101/handouts/assignments/ppmformat.html)
int save_image_ppm(const char *filename, const Image *img) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    // PPM header
    fprintf(fp, "P3\n%d %d\n255\n", img->width, img->height);
    // Pixels
    for (int i = 0; i < img->width * img->height; ++i) {
        fprintf(fp, "%d %d %d\n", img->data[i].r, img->data[i].g,
                img->data[i].b);
    }
    fclose(fp);
    return 0;
}

// Apply a box blur to a strip of the image.
// (https://visionbook.mit.edu/blurring_2.html)
void *blur_task(void *arg) {
    BlurTaskArg *task_arg = (BlurTaskArg *)arg;
    const Image *src = task_arg->src_image;
    Image *dest = task_arg->dest_image;
    int kernel_half = BLUR_KERNEL_SIZE / 2;

    for (int y = task_arg->start_row; y < task_arg->end_row; ++y) {
        for (int x = 0; x < src->width; ++x) {
            unsigned int total_r = 0, total_g = 0, total_b = 0;
            int pixel_count = 0;

            for (int ky = -kernel_half; ky <= kernel_half; ++ky) {
                for (int kx = -kernel_half; kx <= kernel_half; ++kx) {
                    int sample_x = x + kx;
                    int sample_y = y + ky;

                    // Clamp coordinates to be within image bounds
                    if (sample_x >= 0 && sample_x < src->width &&
                        sample_y >= 0 && sample_y < src->height) {
                        Pixel p = src->data[sample_y * src->width + sample_x];
                        total_r += p.r;
                        total_g += p.g;
                        total_b += p.b;
                        pixel_count++;
                    }
                }
            }

            // Calculate the average and write to the destination image
            int dest_index = y * dest->width + x;
            dest->data[dest_index].r = total_r / pixel_count;
            dest->data[dest_index].g = total_g / pixel_count;
            dest->data[dest_index].b = total_b / pixel_count;
        }
    }
    return NULL;
}

void destroy_blur_arg(void *arg) {
    BlurTaskArg *_arg = (BlurTaskArg *)arg;
    if (_arg) {
        free(_arg);
    }
}

int main(int argc, char **argv) {
    struct timeval start, end;
    long duration = 0; // microseconds

    gettimeofday(&start, NULL);

    YATPool *pool = yatpool_init(NUM_THREADS, NUM_THREADS * 2);

    // Prepare images
    printf("Generating a %dx%d test image...\n", IMAGE_WIDTH, IMAGE_HEIGHT);
    Image *src_image = create_image(IMAGE_WIDTH, IMAGE_HEIGHT);
    Image *dest_image = create_image(IMAGE_WIDTH, IMAGE_HEIGHT);
    if (!src_image || !dest_image) {
        fprintf(stderr, "Failed to create images.\n");
        return EXIT_FAILURE;
    }

    // Make source image white to start with
    memset(src_image->data, 255,
           sizeof(Pixel) * src_image->width * src_image->height);

    int worms_per_thread = NUM_WORMS / NUM_THREADS;
    for (int i = 0; i < NUM_THREADS; ++i) {
        WormTaskArg *arg = (WormTaskArg *)malloc(sizeof(WormTaskArg));
        arg->start_worm_no = i * worms_per_thread;
        arg->image = src_image;
        arg->end_worm_no =
            (i == NUM_THREADS - 1) ? NUM_WORMS : (i + 1) * worms_per_thread;

        yatpool_put(pool, &generate_worms, arg, &destroy_worm_task_arg);
    }
    yatpool_wait(pool);

    gettimeofday(&end, NULL);
    duration =
        (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    printf("Generating input image took %g milliseconds.\n",
           (double)duration / 1000.0);

    if (save_image_ppm("input.ppm", src_image) == 0) {
        printf("Input image saved to 'input.ppm'.\n");
    }

    gettimeofday(&start, NULL);

    if (!pool) {
        fprintf(stderr, "Failed to create thread pool.\n");
        return EXIT_FAILURE;
    }

    // Divide work and submit tasks
    printf("Applying a %dx%d box blur using %d threads...\n", BLUR_KERNEL_SIZE,
           BLUR_KERNEL_SIZE, NUM_THREADS);
    int rows_per_thread = IMAGE_HEIGHT / NUM_THREADS;
    for (int i = 0; i < NUM_THREADS; ++i) {
        BlurTaskArg *arg = (BlurTaskArg *)malloc(sizeof(BlurTaskArg));
        arg->src_image = src_image;
        arg->dest_image = dest_image;
        arg->start_row = i * rows_per_thread;
        arg->end_row =
            (i == NUM_THREADS - 1) ? IMAGE_HEIGHT : (i + 1) * rows_per_thread;

        yatpool_put(pool, &blur_task, arg, &destroy_blur_arg);
    }

    yatpool_wait(pool);
    printf("Image processing complete.\n");
    gettimeofday(&end, NULL);
    duration =
        (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    printf("Blurring image took %g milliseconds.\n", (double)duration / 1000.0);

    // Save the result
    if (save_image_ppm("output_blurred.ppm", dest_image) == 0) {
        printf("Blurred image saved to 'output_blurred.ppm'.\n");
    }

    destroy_image(src_image);
    destroy_image(dest_image);
    yatpool_destroy(pool);

    return EXIT_SUCCESS;
}
