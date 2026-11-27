#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>

/* Initialize the OLED and buzzer. */
bool display_init(void);

/* FreeRTOS task that displays incoming Morse characters. */
void display_task(void *argument);

/* Separate task for buzzer sounds so display reading is not delayed. */
void display_buzzer_task(void *argument);

#endif
