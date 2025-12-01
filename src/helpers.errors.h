/**
 * @file helpers.errors.h YARTS utilities.
 * @brief Error return helpers.
 */
#pragma once

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

/** Sets {@link errno} to ERROR_CODE so you don't have to every time. */
static void *null(int error_code) {
    errno = error_code;
    return NULL;
}

static long long zero (int error_code) {
    return (long long) null(error_code);
}

static long long neg1 (int error_code) {
    return zero(error_code) - 1;
}

static int one (int error_code) {
    errno = error_code;
    return 1;
}
