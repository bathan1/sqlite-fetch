/**
 * @file helpers.bhop.h
 * @brief Buffer Handle Open Pipe
 *
 * This is our custom in memory buffer stream over a #bassoon queue.
 */

#include <stdio.h>
#include "bassoon.h"

/**
 * @brief Returns a readable FILE handle bound to BASSOON queue.
 */
FILE *bhop_readable(struct bassoon *bassoon);

/**
 * @brief Returns a writable FILE handle bound to BASSOON queue.
 */
FILE *bhop_writable(struct bassoon *bassoon);
