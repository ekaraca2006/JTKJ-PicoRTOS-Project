#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdbool.h>
#include "FreeRTOS.h"

/* Create the queues used for USB communication. */
bool communication_init(void);

/* Put one allowed Morse character into the transmit queue. */
bool morse_send(char symbol);

/* Read one character received from the workstation. */
bool communication_receive(char *character, TickType_t wait_ticks);

/* FreeRTOS task that sends queued characters through USB CDC1. */
void communication_task(void *argument);

#endif
