/**
 * @file bhop.h
 * @brief Buffer Handle Open Pipe
 *
 * In memory stream that implements FIFO over a #deque
 */
#pragma once
#include <stdio.h>
#include "deque.h"

/**
 * @brief Returns a readable FILE handle bound to DEQUE.
 */
FILE *bhop_readable(struct deque8 *deque);

/**
 * @brief Returns a writable FILE handle bound to DEQUE.
 */
FILE *bhop_writable(struct deque8 *deque);
