#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdbool.h>

#include "FreeRTOS.h"

/* Create the queues used for USB communication. */
bool communication_init(void);

/* Add one outgoing Morse symbol to the queue. */
bool morse_send(char symbol);

/* Get one character received from the workstation. */
bool communication_receive(char *character, TickType_t wait_ticks);

/* FreeRTOS task that sends outgoing Morse symbols. */
void communication_task(void *argument);

#endif
