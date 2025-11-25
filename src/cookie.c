#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <yajl/yajl_parse.h>
#include "cookie.h"

static ssize_t cookie_write(void *c, const char *buf, size_t size)
{
    char *new_buff;
    struct cookie_s *cookie = c;

    /* Buffer too small? Keep doubling size until big enough. */
    while (size + cookie->offset > cookie->allocated) {
        new_buff = realloc(cookie->buf, cookie->allocated * 2);
        if (new_buff == NULL)
            return -1;
        cookie->allocated *= 2;
        cookie->buf = new_buff;
    }

    memcpy(cookie->buf + cookie->offset, buf, size);

    cookie->offset += size;
    if (cookie->offset > cookie->endpos)
        cookie->endpos = cookie->offset;

    return size;
}

static ssize_t
cookie_read(void *c, char *buf, size_t size)
{
    ssize_t xbytes;
    struct cookie_s *cookie = c;

    /* Fetch minimum of bytes requested and bytes available. */
    xbytes = size;
    if (cookie->offset + size > cookie->endpos)
        xbytes = cookie->endpos - cookie->offset;
    if (xbytes < 0)     /* offset may be past endpos */
        xbytes = 0;

    memcpy(buf, cookie->buf + cookie->offset, xbytes);

    cookie->offset += xbytes;
    return xbytes;
}

static int
cookie_seek(void *c, off_t *offset, int whence)
{
    off_t new_offset;
    struct cookie_s *cookie = c;

    if (whence == SEEK_SET)
        new_offset = *offset;
    else if (whence == SEEK_END)
        new_offset = cookie->endpos + *offset;
    else if (whence == SEEK_CUR)
        new_offset = cookie->offset + *offset;
    else
        return -1;

    if (new_offset < 0)
        return -1;

    cookie->offset = new_offset;
    *offset = new_offset;
    return 0;
}

static int cookie_close(void *c)
{
    struct cookie_s *cookie = c;

    free(cookie->buf);
    cookie->allocated = 0;
    cookie->buf = NULL;

    return 0;
}

static cookie_io_functions_t cookie_io_funcs = {
    .read = cookie_read,
    .write = cookie_write,
    .seek = cookie_seek,
    .close = cookie_close
};

FILE *cookies(struct cookie_s *cookie) {
    FILE *stream;
    size_t nread;
    cookie->buf = malloc(INIT_BUF_SIZE);
    if (cookie->buf == NULL) {
        perror("malloc");
        errno = ENOMEM;
        return NULL;
    }
    cookie->allocated = INIT_BUF_SIZE;
    cookie->offset = cookie->endpos = 0;
    stream = fopencookie(cookie, "w+", cookie_io_funcs);
    if (stream == NULL) {
        perror("fopencookie");
        errno = EIO;
        return NULL;
    }
    return stream;
}

static size_t flush(FILE *mem, size_t *size) {
    size_t prev_size = *size;
    int rc = fflush(mem);
    if (rc == EOF) {
        return EOF;
    }
    size_t offset = *size - prev_size;
    return offset;
}

const char *cookie_next(cookie_t *cookie, FILE *cookie_stream) {
    size_t prev = cookie->endpos;
    size_t num_flushed = flush(cookie_stream, &cookie->endpos);
    if (num_flushed == EOF) {
        errno = EOF;
        return NULL;
    }
    if (num_flushed > 0) {
        return cookie->buf + prev;
    }
    return NULL;
}
