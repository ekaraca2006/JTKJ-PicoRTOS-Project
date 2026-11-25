#include <stdio.h>
#include "communication.h"

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <tusb.h>

#define MORSE_QUEUE_LENGTH 64
#define CDC_ITF_TX 1

// Queue for transferring Morse symbols between tasks
static QueueHandle_t morseQueue = NULL;


// Initialize the Morse communication queue
void communication_init(void)
{
    morseQueue = xQueueCreate(
        MORSE_QUEUE_LENGTH,
        sizeof(char)
    );
}


// Send one Morse symbol to the queue
bool morse_send(char symbol)
{
    if (morseQueue == NULL) {
        return false;
    }

    // Only accept valid Morse protocol characters
    if (symbol != '.' &&
        symbol != '-' &&
        symbol != ' ' &&
        symbol != '\n') {
        return false;
    }

    return xQueueSend(
        morseQueue,
        &symbol,
        0
    ) == pdPASS;
}


// Send one character through USB CDC
static bool send_usb_char(char symbol)
{
    // Check whether the USB device is mounted and CDC 1 is connected
    if (!tud_mounted() || !tud_cdc_n_connected(CDC_ITF_TX)) {
        return false;
    }

    // Wait for available TX buffer space, but do not wait forever
    for (int i = 0; i < 100; i++) {

        if (!tud_mounted() ||
            !tud_cdc_n_connected(CDC_ITF_TX)) {
            return false;
        }

        if (tud_cdc_n_write_available(CDC_ITF_TX) > 0) {
            uint32_t written = tud_cdc_n_write(
                CDC_ITF_TX,
                &symbol,
                1
            );

            tud_cdc_n_write_flush(CDC_ITF_TX);

            return written == 1;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return false;
}


// Communication task
void communication_task(void *arg)
{
    (void)arg;

    char symbol;

    while (1) {

        // Wait for a Morse symbol from the queue
        if (xQueueReceive(
                morseQueue,
                &symbol,
                portMAX_DELAY) == pdPASS) {

            // Newline indicates the end of a message.
            // Add two spaces followed by line feed.
            if (symbol == '\n') {
                if (!send_usb_char(' ')) continue;
                if (!send_usb_char(' ')) continue;
                send_usb_char('\n');
            }
            else {
                send_usb_char(symbol);
            }
        }
    }
}