#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdbool.h>
#include "FreeRTOS.h"

/* Set up the queues used for USB messages. */
bool communication_init(void);

/* Add one Morse character to the sending queue. */
bool morse_send(char character);

/* Get one character received from the workstation. */
bool communication_receive(char *character, TickType_t wait_time);

/* FreeRTOS task that sends characters to the workstation. */
void communication_task(void *argument);

#endif
