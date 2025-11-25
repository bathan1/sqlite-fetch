#pragma once

/**
 * Connects to host at URL over tcp, then returns the socket fd
 * *after* sending the corresponding HTTP request encoded in URL.
 */
int fetch(const char *url);
