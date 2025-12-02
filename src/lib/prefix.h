/**
 * @file prefix.h
 * @brief Hack string in C
 *
 * Primary motivation behind this is to be able to lookup length
 * in constant time after computing \c strlen() on it once **without**
 * having to define a new struct ourselves.
 */
#include <stddef.h>

/**
 * Flattened struct type that has `sizeof(size_t)` SIZE prefix bytes
 * at the head, and then has SIZE + 1 bytes after for the string + nul terminator.
 *
 * For example, "hi" on big endian on most 64 bit machines would be encoded as:
 *
 * |----0-----|----1----| ... |----7----|----8----|----9----|----10----|
 * |    0     |    0    | ... |    2    |   'h'   |   'i'   |    0     |
 */
typedef char pre;

/**
 * @brief Get a *const* pointer to the string portion of a pre string.
 *
 * Labeled pointer prefix addition of \c sizeof(size_t) to S
 * so you don't need to remember to add it each time.
 *
 * It is \c const because you can't \c free() it -- you have to
 * free from the head.
 */
#define str(s) ((const char *)(s + sizeof(size_t)))

/** Constant time size lookup that does the type casting for you. */
#define len(buf) (*(size_t *)buf)

/**
 * @brief Allocate a pre string S of known length LEN.
 *
 * *Copies* S, so you can free S after this.
 *
 * @retval SOME_BUFFER OK. Wrote out successfully.
 * @retval NULL When out of memory
 *
 * # Errors
 * - `ENOMEM` - Out of memory.
 */
pre *prefix_static(const char *s, size_t len);

/**
 * @brief Allocate a pre string with the given format string FMT.
 *
 * It's like \c sprintf() except that it *returns* the buffer rather than making you pass it in.
 * FMT can be freed after this.
 *
 * @retval SOME_BUFFER OK. Wrote out successfully.
 * @retval NULL Out of heap memory.
 *
 * # Errors
 * - `EINVAL` - Malformed format string FMT.
 * - `ENOMEM` - Out of memory.
 */
pre *prefix(const char *fmt, ...);

/**
 * Returns a *new* string with CH removed from *length pre* STR,
 * Sets N to the new size if it is passed in. Otherwise, it will
 * compute `strlen` on STR to handle the removal.
 *
 * @retval SOME_BUFFER OK.
 */
pre *rmch(const pre *str, char ch);

/** @brief #rmch() but it takes ownership of the S. */
pre *rmch_own(pre *s, char ch);

/**
 * @brief Split S on CH into TOKEN_COUNT tokens.
 *
 * Writes out total number of tokens to TOKEN_COUNT, if the ptr is not `NULL`.
 */
pre **split_on_ch(const char *s, char ch, size_t *token_count);

/** @brief Convert a pre string S to prefixed lowercase. */
pre *lowercase(const pre *s);

/** @brief #lowercase() but it takes ownership of the S. */
pre *lowercase_own(pre *s);
