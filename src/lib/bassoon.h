/**
 * @file bassoon.h
 * @brief A(nother) JSON object queue
 *
 * `bassoon.h` exports a pipe that connects a byte stream write end
 * with a self-framing queue (#bhop()) along with the internal queue functions
 * associated with the actual #bassoon struct.
 *
 * You'll probably never need to use the bassoon functions yourself,
 * but they're exported here for debugging / testing.
 *
 */
#pragma once

/**
 * @brief A double ended queue for JSON values.
 *
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

/**
 * @brief Initializes the queue at BASS.
 *
 * Allocates the #bassoon::buffer at BASS on the heap, along
 * with setting the initial fields.
 */
void bassoon_init(struct bassoon *bass);

/**
 * @brief Push object buffer at VAL into BASS.
 *
 * VAL is expected to be a well-formed JSON object because the
 * partial byte handling is left to a parent of BASS to handle.
 */
void bassoon_push(struct bassoon *bass, char *val);

/**
 * @brief Free the queue buffer at BASS.
 *
 * Also frees BASS, so accessing BASS after calling
 * #bassoon_free() is UB.
 */
void bassoon_free(struct bassoon *bass);

/**
 * @brief Pop an object from the queue in BASS.
 *
 * #bassoon::count is decremented accordingly assuming
 * the queue is nonempty.
 *
 * @retval NULL Empty. `BASS->count = 0`.
 * @retval ~0 Success (anything but 0 / NULL).
 */
char *bassoon_pop(struct bassoon *bass);
