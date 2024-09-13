# Examples for `yatpool`

Two examples are included to show the functions of `yatpool`. 

- Numerical integration: $y = 9-x^{2}$ is integrated between $x = 0$ and $x = 3$. The area is evaluated using Monte Carlo simulations.
- Writing data to a CSV: Random integers are generated for a given number of rows and written to a CSV.

## How to build

First build `yatpool` using the instructions on the top-level readme. Then run

```
make
```

The binaries are built and stored in the folder `bin`. To clean builds run

```
make clean
```

## How to run

Number of threads for the threaded versions can be changed in the source.

### Numerical integration

The number of iterations must be specified when running the binaries. Please specify at least 1 million iterations.

```
./numerical_integration_serial 1000000
./numerical_integration_threaded 1000000
```

### Writing data to a CSV

The file to write to and the number of rows of data to generated should be specified when running the binaries.
```
./writing_to_file_serial foo.csv 1000000
./writing_to_file_threaded bar.csv 1000000
```
