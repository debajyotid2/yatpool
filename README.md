# YATPool

Yet another [thread pool](https://en.wikipedia.org/wiki/Thread_pool) implementation in pure C using POSIX threads.

## Features

- Simple API.

## Example usage

This snippet from [numerical_integration_threaded.c](./examples/numerical_integration_threaded.c) shows instantiation, task addition and termination of a thread pool for finding the area under the curve of a function.

```c
#include <stdlib.h>
#include "yatpool.h"

...
    double x_low = 0.0, x_high = 3.0;
    double y_low = func(x_low);
    double y_high = func(x_high);

    size_t* hits = (size_t*)calloc(num_threads, sizeof(size_t));
    
    // Initialize thread pool
    YATPool* pool;
    yatpool_init(&pool, num_threads, num_threads);
    
    for (size_t i=0; i<num_threads; ++i) {
        // Create task
        Task* task;
        HitCtrArg* arg;
        hitctrarg_init(&arg, x_low, x_high, y_low, y_high, num_its, &hits[i]);
        task_init(&task, &count_hits, arg, &hitctrarg_destroy);
        
        // Submit task to the thread pool
        yatpool_put(pool, task);
    }
    
    // Wait for all tasks to be finished
    yatpool_wait(pool);

    // Destroy the thread pool
    yatpool_destroy(pool);

    size_t total_hits = 0;
    for (size_t i=0; i<num_threads; ++i)
        total_hits += hits[i];
 ...
```

## How to build/install

Only Linux-based operating systems are supported as of now.

`g++` (v.11.4.0 and above) is the compiler to be used for compiling the project. If not already installed, `gcc` can be downloaded from the package repository of the target Linux distribution (e.g. `apt` for Ubuntu) or [built from source](https://gcc.gnu.org/install/). `CMake` (v.3.27 and above) is used for building the library, and can also be installed (if available) from the package repository (e.g. `apt` for Ubuntu) or [built from source](https://cmake.org/download/).

Once the dependencies (`gcc` and `cmake`) are installed, first [clone the repository](https://docs.github.com/en/repositories/creating-and-managing-repositories/cloning-a-repository#cloning-a-repository).

### Building

To build `yatpool`, please run

```
cd scripts
source build.sh
```

`libyatpool.a` is the static library built here, which can be found in the default build directory (`build/src`). Then the static library can be directly linked while compiling any project.

### Installation

To install `yatpool` to your system, please run
```
source install.sh
```

## How to include in a project

Once `yatpool` has been built, a "preferred" project setup is as follows:
```
├── include
│   └── yatpool.h
├── lib
│   └── libyatpool.a
└── src
    └── foo.c
...
```
With the above setup, it is easy to link `yatpool` when building your project, as follows.
```
g++ -I./include -L./lib foo.c -o foo -lyatpool
```

## License

[GPL v3.0](https://www.gnu.org/licenses/gpl-3.0.en.html)

