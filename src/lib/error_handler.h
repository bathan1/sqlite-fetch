/**
 * Plaster a \c perror() call with argument TAG, running
 * cleanup code ... before returning return code RC.
 */
#define perror_rc(__rc, __tag, ...)                              \
    (perror(__tag), __VA_ARGS__, (__rc))
