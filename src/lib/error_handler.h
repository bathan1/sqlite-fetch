/**
 * @file error_handler.h
 * @brief Common error handling macros / functions
 */

/**
 * @brief Call perror(TAG), run variadic cleanup code, then return RC
 */
#define perror_rc(__rc, __tag, ...)                              \
    (perror(__tag), __VA_ARGS__, (__rc))
