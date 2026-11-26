#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "tusb.h"
#include "tkjhat/sdk.h"

#include "communication.h"

#define TX_QUEUE_SIZE 256
#define RX_QUEUE_SIZE 512
#define MORSE_USB_PORT 1

/* CDC0 is for debugging. Morse messages use CDC1. */
static QueueHandle_t tx_queue = NULL;
static QueueHandle_t rx_queue = NULL;

bool communication_init(void)
{
    tx_queue = xQueueCreate(TX_QUEUE_SIZE, sizeof(char));
    if (tx_queue == NULL) {
        return false;
    }

    rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(char));
    if (rx_queue == NULL) {
        vQueueDelete(tx_queue);
        tx_queue = NULL;
        return false;
    }

    return true;
}

bool morse_send(char character)
{
    if (tx_queue == NULL) {
        return false;
    }

    if (character != '.' && character != '-' &&
        character != ' ' && character != '\n') {
        return false;
    }

    /* This function is called by a task, so a normal queue send is used. */
    return xQueueSend(tx_queue, &character, pdMS_TO_TICKS(50)) == pdPASS;
}

bool communication_receive(char *character, TickType_t wait_time)
{
    if (rx_queue == NULL || character == NULL) {
        return false;
    }

    return xQueueReceive(rx_queue, character, wait_time) == pdPASS;
}

/* Send one character. Wait until the workstation opens the Morse port. */
static void send_usb_character(char character)
{
    for (;;) {
        if (tud_mounted() && tud_cdc_n_connected(MORSE_USB_PORT)) {
            if (tud_cdc_n_write_available(MORSE_USB_PORT) > 0) {
                uint32_t written = tud_cdc_n_write(
                    MORSE_USB_PORT,
                    &character,
                    1
                );

                if (written == 1) {
                    tud_cdc_n_write_flush(MORSE_USB_PORT);
                    return;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* Flash the red LED twice after the whole message has been sent to USB. */
static void show_send_finished(void)
{
    int count;

    for (count = 0; count < 2; count++) {
        set_red_led_status(true);
        vTaskDelay(pdMS_TO_TICKS(120));
        set_red_led_status(false);
        vTaskDelay(pdMS_TO_TICKS(120));
    }
}

void communication_task(void *argument)
{
    char character;

    (void)argument;

    for (;;) {
        if (xQueueReceive(tx_queue, &character, portMAX_DELAY) == pdPASS) {
            send_usb_character(character);

            if (character == '\n') {
                show_send_finished();
            }
        }
    }
}

/* TinyUSB calls this from the USB task when data arrives. */
void tud_cdc_rx_cb(uint8_t interface_number)
{
    char buffer[64];
    uint32_t count;
    uint32_t i;

    /* Read and ignore data sent to the debug port. */
    if (interface_number != MORSE_USB_PORT) {
        while (tud_cdc_n_available(interface_number) > 0) {
            (void)tud_cdc_n_read(interface_number, buffer, sizeof(buffer));
        }
        return;
    }

    if (rx_queue == NULL) {
        return;
    }

    while (tud_cdc_n_available(MORSE_USB_PORT) > 0) {
        count = tud_cdc_n_read(MORSE_USB_PORT, buffer, sizeof(buffer));

        if (count == 0) {
            break;
        }

        for (i = 0; i < count; i++) {
            /* Do not wait here. The USB task must keep running. */
            (void)xQueueSend(rx_queue, &buffer[i], 0);
        }
    }
}
