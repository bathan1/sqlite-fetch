/**
 * @file yarts.h
 * @brief Yet Another Runtime TCP Stream
 *
 * Helper functions #yarts.c has available.
 *
 * @example bassoon_print.c
 * `gcc bassoon_print.c -lyarts -o bassoon_print`
 *
 * @example fetch_print.c
 * `gcc fetch_print.c -lyarts -o fetch_print`
 */

 #include <stdio.h>

 /* FETCH FRAME OPTIONS. These are just plain integers
  * that are statically cast to `const char *` for convenience.
  * You can pass in plain integers to the frame slot and that will
  * work just fine, you will just get some compiler warnings. */

/**
 * @brief Parse JSON objects into NDJSON.
 * This is also the default parse strategy.
 */
static const char *FRAME_NDJSON = 0;

/**
 * @brief Initialize a write/read pipe over a #bassoon queue.
 *
 * Bassoon Handle Open Pipe, or #bhop(), opens a unidirectional
 * pipe where you write into `FILES[0]` and read from `FILES[1]`,
 * just like posix pipes.
 *
 * @retval  0  Success. `FILES[0]` and `FILES[1]` are fully initialized.
 * @retval -1  Error. `FILES` is left unchanged and errno is set.
 *
 * ### Example with manual JSON
 * @snippet bassoon_print.c bhop basic usage
 */
int bhop(FILE *files[2]);

/**
 * @brief \c send() HTTP Request over a TCP socket, wrapping the response socket over
 * the returned `FILE *` stream.
 *
 * Connects to host at URL over tcp and writes optional HTTP fields in INIT
 * to the request. It returns a readable FILE stream that separates the frames
 * by newlines, so you can read each logical frame one by one easier.
 *
 * INIT slots are:
 *  - [0]: Method case insensitive
 *  - [1]: Headers
 *  - [2]: Body
 *  - [3]: *plain int* Body parser frame type.
 *
 * INIT[3] is the only slot that #fetch will read as a plain
 * `uint64`.
 *
 * @retval NOT_0 OK - Anything not 0 means the response stream was successfully opened.
 * @retval NULL Error - Check `errno` to learn about the error (too many to list here).
 *
 * ### Typicode API Example
 * @snippet fetch_print.c fetch basic usage
 */
FILE *fetch(const char *url, const char *init[4]);
