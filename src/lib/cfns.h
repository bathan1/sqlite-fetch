/**
 * @file cfns.h
 * @brief Common C functions for the other modules
 */

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Buffer and length
 *
 * So you don't have to \c strlen() / remember to store the size
 * if you don't want to recompute.
 */
struct string {
    char *hd;
    size_t length;
};

/**
 * @brief Allocate a string with the given format string FMT.
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
struct string dynamic(const char *fmt, ...);

/**
 * @brief Slice a *copy* of S from START to END
 */
struct string slice(const struct string s, size_t start, size_t end);

/**
 * @brief Duplicate S and its #string::hd.
 */
struct string stringdup(const struct string s);

/**
 * @brief Split S wherever PATTERN occurs into NTOKENS tokens.
 */
struct string *split(const struct string s, const struct string pattern, size_t *ntokens);

/**
 * @brief Split S wherever DELIM is into NTOKS tokens.
 */
struct string *splitch(const struct string s, char delim, size_t *ntoks);

/**
 * @brief Convert S into lowercase in place.
 *
 * @retval 0 On success, all characters are lowercased.
 * @retval 1 On failure, S couldn't be resolved.
 */
int lowercase(struct string *s);

/**
 * @brief Remove all occurrences of CH from S.
 *
 * @retval 0 On success, all instances of CH removed.
 * @retval 1 On failure, S couldn't be resolved.
 */
int rmch(struct string *s, char ch);

/**
 * @brief #lowercase() but mutates the returned COPY of S.
 */
struct string lowercase_im(const struct string s);

/** @brief min macro */
#define MIN(a, b) ((a < b) ? a : b)
/** @brief max macro */
#define MAX(a, b) ((a > b) ? a : b)

/**
 * @brief Call perror(TAG), run variadic cleanup code, then return RC
 */
#define perror_rc(__rc, __tag, ...)                              \
    (perror(__tag), __VA_ARGS__, (__rc))
