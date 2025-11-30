/** 
 * @file bassoon.h Bassoon JSON Queue
 * @brief A JSON deque wrapper over byte streams.
 *
 * `bassoon.h` exports a queue pipe that connects byte stream
 * writes into a NDJSON readable FILE end, along with
 * the internal queue functions associated with the actual
 * #bassoon struct.
 *
 * You'll probably never need to use the bassoon functions yourself,
 * but they're exported here for debugging / testing.
 *
 * @example examples/c/bassoon_print.c
 * Demonstrates how to use bassoon streaming JSON.
 *
 * Compile with:
 * ```bash
 * gcc bassoon_print.c -lyarts
 * ```
 *
 * See the [README](README.md) on installing onto the usr lib.
 */
#pragma once

#include <stdio.h>

/**
 * Bassoon Handle Open Pipe, or bhop, opens a unidirectional
 * pipe where you write into `FILES[0]` and read from `FILES[1]`,
 * just like posix pipes.
 *
 * @retval  0  Success. `FILES[0]` and `FILES[1]` are fully initialized.
 * @retval -1  Error. `FILES` is left unchanged and errno is set.
 */
int bhop(FILE *files[2]);

/**
 * A JSON object queue with a writable file descriptor. You can write 
 * whatever bytes you want to it, like the bytes from a TCP stream.
 *
 * Reads are constant-time from both its head and its tail, so it's technically a "deque".
 */
struct bassoon {
    /** The actual buffer. */
    char **buffer;

    /** #buffer capacity, i.e. read/write at `BUFFER[x >= CAP]` is UB. */
    unsigned long cap;

    /** First in end. */
    unsigned long hd;

    /** Last in end. */
    unsigned long tl;

    /** Stored size. Is updated dynamically from calls to #bassoon_pop. */
    unsigned long count;
};

/** Initializes the queue at BASS. */
void bassoon_init(struct bassoon *bass);

/** Push object buffer at VAL into BASS. */
void bassoon_push(struct bassoon *bass, char *val);

/** Free the queue buffer at BASS. */
void bassoon_free(struct bassoon *bass);

/** Pop an object from the queue in BASS, or NULL if it's empty. */
char *bassoon_pop(struct bassoon *bass);

