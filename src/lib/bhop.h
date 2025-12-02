/**
 * @file bhop.h
 * @brief Buffer Handle Open Pipe
 *
 * This is our custom in memory buffer stream over a #bassoon queue.
 */
#pragma once
#include <stdio.h>
#include "deque.h"

/**
 * @brief Returns a readable FILE handle bound to BASSOON queue.
 */
FILE *bhop_readable(struct deque8 *bassoon);

/**
 * @brief Returns a writable FILE handle bound to BASSOON queue.
 */
FILE *bhop_writable(struct deque8 *bassoon);
