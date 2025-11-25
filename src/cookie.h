#pragma once

#include <unistd.h>
#include <stdio.h>
#define INIT_BUF_SIZE 4

struct cookie_s {
    char *buf;
    size_t allocated;
    size_t endpos;
    off_t offset;
};
typedef struct cookie_s cookie_t;

#define cookie_alloc() calloc(1, sizeof(struct cookie_s))

/**
 * Get the FILE handle from COOKIE. If there is an error,
 * this returns NULL and sets `errno` accordingly.
 */
FILE *cookies(struct cookie_s *cookie);
