/**
 * @file deque.h
 * @brief Double ended queue implementation.
 */
#pragma once

/**
 * @brief A double ended queue for uint8 pointers (aka char).
 *
 * Reads are constant-time from both its head and its tail, so it's technically a "deque".
 */
struct deque8 {
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

/**
 * @brief Initializes the dequeue at DEQUE.
 *
 * Allocates the #deque::buffer at DEQUE on the heap, along
 * with setting the initial fields.
 */
void deque8_init(struct deque8 *deque);

/**
 * @brief Push object buffer at VAL into DEQUE.
 */
void deque8_push(struct deque8 *deque, char *val);

/**
 * @brief Free the queue buffer at DEQUE.
 *
 * Also frees DEQUE, so accessing DEQUE after calling
 * #deque8_free() is UB.
 */
void deque8_free(struct deque8 *deque);

/**
 * @brief Pop an object from the queue in DEQUE.
 *
 * #deque8::count is decremented accordingly assuming
 * the queue is nonempty.
 *
 * @retval NULL Empty. `BASS->count = 0`.
 * @retval ~0 Success (anything but 0 / NULL).
 */
char *deque8_pop(struct deque8 *deque);
