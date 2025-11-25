#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int walk(
    const char *json_buf,
    const char *path,
    char ***out_items,
    size_t *out_count
);

#ifdef __cplusplus
}
#endif

