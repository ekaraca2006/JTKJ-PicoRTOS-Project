#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>

/* Set up the OLED display and buzzer. */
bool display_init(void);

/* FreeRTOS task that receives and displays Morse characters. */
void display_task(void *argument);

/* FreeRTOS task that plays the received Morse characters. */
void buzzer_task(void *argument);

#endif
