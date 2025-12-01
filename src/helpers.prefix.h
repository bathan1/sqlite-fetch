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
typedef char prefixed;

/**
 * Labeled pointer prefix addition of \c sizeof(size_t) to S
 * so you don't need to remember to add it each time.
 */
#define str(s) (s + sizeof(size_t))

/** Constant time size lookup that does the type casting for you. */
#define len(buf) (*(size_t *)buf)

/**
 * @brief Allocate a prefixed string S of known length LEN.
 */
prefixed *prefix_static(const char *s, size_t len);

/**
 * @brief Allocate a prefixed string with the given format string FMT.
 *
 * Like \c sprintf() except that it *returns* the buffer rather than making you pass it in.
 * FMT can be freed after this.
 */
prefixed *prefix(const char *fmt, ...);

/**
 * Returns a *new* string with CH removed from *length prefixed* STR,
 * Sets N to the new size if it is passed in. Otherwise, it will
 * compute `strlen` on STR to handle the removal.
 */
prefixed *remove_all(const prefixed *str, char ch);
/**
 * @brief Split S on CH into TOKEN_COUNT tokens.
 *
 * Writes out total number of tokens to TOKEN_COUNT, if the ptr is not `NULL`.
 */
prefixed **split_on_ch(const char *s, char ch, size_t *token_count);

/** @brief Convert a prefixed string S to prefixed lowercase. */
prefixed *lowercase(prefixed *s);
