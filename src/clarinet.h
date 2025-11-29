/**
 * @file clarinet.h
 * @brief A JSON deque for byte streams
 */

#pragma once

#include <stdio.h>

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif
#define MAX_DEPTH 64

/**
 * A JSON object queue with a writable file descriptor. You can write whatever bytes you want to it...
 *
 * ...like bytes from a TCP stream.
 *
 * Reads are constant-time from both its head and its tail,
 * so it's technically a "deque".
 */
typedef struct clarinet {
    /**
     * Underlying buffer that #clarinet will free via #clarinet_free.
     */
    char **buffer;

    /**
     * #buffer capacity, i.e. read/write at `BUFFER[x >= CAP]` is UB.
     */
    unsigned long cap;

    /**
     * First in end.
     */
    unsigned long hd;

    /**
     * Last in end.
     */
    unsigned long tl;

    /**
     * Stored size.
     */
    unsigned long count;

    /**
     * Plain writable "stream" of the queue so you can just #fwrite bytes to the queue.
     */
    FILE *writable;
} clarinet_t;

/**
 * Allocate a clarinet handle on the heap.
 */
struct clarinet *use_clarinet();

/**
 * Free the clarinet buffer at CLARE.
 */
void clarinet_free(struct clarinet *clare);

/**
 * Pop an object from the queue in CLARE, or NULL if it's empty.
 */
char *clarinet_pop(struct clarinet *clare);
