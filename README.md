# Pthread-Parallel-PageRank

A C implementation of an iterative PageRank-style graph computation parallelized with POSIX threads. The assignment explores shared-memory parallelism: threads dynamically claim vertices, compute their incoming-link contributions, and synchronize between iterations with mutexes and barriers.

## Overview

The program reads a directed graph from a text file, represents each node by its incoming neighbors, and repeatedly computes a score for every node. Work is shared dynamically among a caller-selected number of worker threads. The program repeats the computation five times, reports the elapsed CPU time for each run plus the mean and standard deviation, and writes the last run's scores to `pagerank.csv`.

## Parallel workflow

1. **Read the graph.** `read_input` scans the edge list to find the largest node ID and count incoming edges. It allocates an incoming-neighbor array for each node, then reads the file again to populate those arrays and compute each node's out-degree.
2. **Initialize a run.** `run_pagerank` initializes the score arrays, creates the requested number of pthread workers, and joins them after all iterations finish.
3. **Distribute vertices dynamically.** At the start of each iteration, workers synchronize at a barrier. Each worker locks a shared mutex, increments `next_node` to claim a vertex, and releases the mutex. It then computes that vertex's score from its incoming neighbors. Since each vertex is claimed once per iteration, workers write to distinct entries in `new_pagerank`.
4. **Synchronize score updates.** A barrier ensures all vertices have been processed before a worker copies the newly computed values into the current score array. The shared work counter is reset under the mutex, and another barrier ensures the reset is complete before the next iteration.
5. **Report and save results.** After five runs, the program writes node IDs and scores to `pagerank.csv` and reports timing statistics.

This design demonstrates dynamic work assignment with a shared counter, mutex-protected state, and barrier-based phase synchronization. The trade-off is that every vertex claim requires locking the same mutex, which can become contention as thread counts increase.

## Graph input format

The input is a whitespace-separated directed edge list. Each line with two integers represents an edge from the first node to the second:

```text
0 1
0 2
1 2
```

Lines beginning with `#` are ignored. Node IDs are used as array indexes, so the implementation expects non-negative, reasonably dense IDs starting at zero. The largest ID determines the allocated range, including IDs that have no incident edges.

## Build and run

Compile with GCC and pthread support:

```bash
gcc -O2 -o pagerank page_rank.c -pthread -lm
```

Run from the project directory:

```bash
./pagerank <input_file> <number_of_threads>
```

For example:

```bash
./pagerank graph.txt 4
```

On Windows, use a GCC environment that supports POSIX threads, such as WSL or MinGW-w64 configured with pthread support. The output file is always named `pagerank.csv` in the current working directory; an existing file with that name is overwritten.

## Source and accompanying material

- `page_rank.c` implements graph loading, PageRank-style iteration, pthread worker coordination, timing, and CSV output.
- `MY_README.txt` contains the original brief build and run commands.
- `report.pdf` and `report.odt` are the accompanying assignment report in PDF and OpenDocument formats.
- `.vscode/` contains editor and C/C++ debugging configuration.

## Implementation notes

This is an educational assignment implementation, and its current behavior has limitations worth knowing when interpreting results:

- The update is `0.15 + 0.85 × incoming contribution`; the `0.15` term is not divided by the number of nodes, so scores are not normalized in the conventional PageRank manner.
- It runs a fixed 50 iterations rather than testing for convergence.
- Dangling-node rank is not redistributed.
- The score-copy operation copies `nodes` entries although the arrays cover IDs `0` through `nodes` inclusive. Consequently, the largest-ID node is not copied into the next iteration's score vector.
- The reported duration uses C's `clock()`, which measures process CPU time; it is not a reliable wall-clock measure for comparing parallel speedup.
- Node IDs are assumed to be dense and non-negative. Very sparse IDs can cause unnecessarily large allocations.

These points describe the current source code; they should be addressed before treating the output as a validated standard PageRank result or using the timing as a parallel performance benchmark.
