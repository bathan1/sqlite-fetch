/**
 * @file cfns.h
 * @brief Common C functions for the other modules
 */

/** @brief min macro */
#define MIN(a, b) ((a < b) ? a : b)
/** @brief max macro */
#define MAX(a, b) ((a > b) ? a : b)

/**
 * @brief Call perror(TAG), run variadic cleanup code, then return RC
 */
#define perror_rc(__rc, __tag, ...)                              \
    (perror(__tag), __VA_ARGS__, (__rc))
