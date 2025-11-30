/** 
 * @file bassoon.h Bassoon JSON Queue
 * @brief A JSON deque wrapper over byte streams.
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

    /** Stored size. Is updated dynamically from calls to #bass_pop. */
    unsigned long count;

    /** Plain writable "stream" of the queue so you can just \c fwrite() on it. */
    FILE *writable;
};

/** Dynamically allocate a bassoon queue. */
struct bassoon *Bassoon();

/** Free the queue buffer at BASS. */
void bass_free(struct bassoon *bass);

/** Pop an object from the queue in BASS, or NULL if it's empty. */
char *bass_pop(struct bassoon *bass);
