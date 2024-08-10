# Test-TLB

`test-tlb` is a performance testing program designed to evaluate memory access patterns and the efficiency of the Translation Lookaside Buffer (TLB). The application measures the time it takes to access memory under different configurations and provides insights into TLB performance.

## Table of Contents
- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
- [Tests](#tests)
- [Makefile Usage](#makefile-usage)
- [Known Issues](#known-issues)

## Features

- **Huge Pages**: Option to use huge pages for the test.
- **Random List**: Option to use random access patterns.
- **Timing Measurement**: Measures access time in nanoseconds and estimates cycle time.

## Installation

1. **Clone the Repository**:
   ```sh
   git clone https://github.com/torvalds/test-tlb.git
   cd test-tlb
   ```

2. **Build the Project**:
   ```sh
   make
   ```

   This command will compile the `test-tlb` application and create the `test-tlb` executable.

## Usage

The `test-tlb` program operates with two main parameters: memory size and stride length. To run the test, use the following command:

```sh
./test-tlb <size> <stride>
```

- `<size>`: The size of the memory area. Examples: `4k`, `16k`, `1M`, etc.
- `<stride>`: The stride length for memory access. Examples: `4k`, `16k`, `512k`, etc.

### Options

- `-H`: Use huge pages for the test.
- `-r`: Use a random access pattern.

Example commands:

```sh
./test-tlb 4k 4k
./test-tlb -H 4k 4k
./test-tlb -r 16k 16k
```

## Tests

The `Makefile` includes commands for running various tests with different memory sizes and stride lengths. To execute all tests, use:

```sh
make run
```

This command will run a series of tests with predefined configurations and print the results to the console.

## Makefile Usage

- **Build**: `make` will compile the `test-tlb` target.
- **Clean**: `make clean` will remove all build artifacts.
- **Run Tests**: `make run` will execute the defined tests and display results.

## Known Issues

- **Huge Pages**: The `MADV_HUGEPAGE` and `MADV_NOHUGEPAGE` macros may not be defined on all systems. This may lead to compatibility issues on systems where these features are not supported.

## License

This project is licensed under the [GPLv2](LICENSE.txt).
