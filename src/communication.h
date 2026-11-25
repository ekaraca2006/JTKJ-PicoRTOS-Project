
#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdbool.h>
#include <stdint.h>

// Initialize the Morse communication system
void communication_init(void);

// Send one Morse symbol to the communication queue
bool morse_send(char symbol);

// FreeRTOS communication task
void communication_task(void *arg);

#endif
