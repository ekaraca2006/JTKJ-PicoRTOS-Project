#ifndef MORSE_H
#define MORSE_H

#include <stdbool.h>

/* Start the accelerometer and gyroscope. */
bool morse_init(void);

/* FreeRTOS task that reads tilts and sends Morse characters. */
void morse_task(void *argument);

#endif
