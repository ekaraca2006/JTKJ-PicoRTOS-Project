#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "tusb.h"
#include "tkjhat/sdk.h"

#include "communication.h"

#define MORSE_TX_QUEUE_LENGTH 128
#define MORSE_RX_QUEUE_LENGTH 256
#define MORSE_CDC_INTERFACE   1

/* CDC0 is used for debug output. Morse messages use CDC1. */
static QueueHandle_t tx_queue = NULL;
static QueueHandle_t rx_queue = NULL;

bool communication_init(void)
{
    tx_queue = xQueueCreate(MORSE_TX_QUEUE_LENGTH, sizeof(char));
    rx_queue = xQueueCreate(MORSE_RX_QUEUE_LENGTH, sizeof(char));

    if (tx_queue == NULL || rx_queue == NULL) {
        if (tx_queue != NULL) {
            vQueueDelete(tx_queue);
            tx_queue = NULL;
        }

        if (rx_queue != NULL) {
            vQueueDelete(rx_queue);
            rx_queue = NULL;
        }

        return false;
    }

    return true;
}

bool morse_send(char symbol)
{
    if (tx_queue == NULL) {
        return false;
    }

    if (symbol != '.' && symbol != '-' && symbol != ' ' && symbol != '\n') {
        return false;
    }

    /* This function is called from a task, not from an interrupt. */
    return xQueueSend(tx_queue, &symbol, pdMS_TO_TICKS(100)) == pdPASS;
}

bool communication_receive(char *character, TickType_t wait_ticks)
{
    if (rx_queue == NULL || character == NULL) {
        return false;
    }

    return xQueueReceive(rx_queue, character, wait_ticks) == pdPASS;
}

/* Send one byte on the Communication CDC port. */
static void send_usb_character(char character)
{
    for (;;) {
        if (!tud_mounted() || !tud_cdc_n_connected(MORSE_CDC_INTERFACE)) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (tud_cdc_n_write_available(MORSE_CDC_INTERFACE) > 0) {
            uint32_t written = tud_cdc_n_write(
                MORSE_CDC_INTERFACE,
                &character,
                1
            );

            if (written == 1) {
                tud_cdc_n_write_flush(MORSE_CDC_INTERFACE);
                return;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void indicate_transmission_complete(void)
{
    /* This confirms that bytes were written to USB, not that the host decoded them. */
    for (int i = 0; i < 2; ++i) {
        set_red_led_status(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        set_red_led_status(false);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void communication_task(void *argument)
{
    (void)argument;

    char symbol;

    for (;;) {
        if (xQueueReceive(tx_queue, &symbol, portMAX_DELAY) != pdPASS) {
            continue;
        }

        send_usb_character(symbol);

        if (symbol == '\n') {
            indicate_transmission_complete();
        }
    }
}

/* TinyUSB calls this from the task that runs tud_task(). */
void tud_cdc_rx_cb(uint8_t interface_number)
{
    char buffer[64];

    if (interface_number != MORSE_CDC_INTERFACE) {
        /* Drain CDC0 so its receive buffer cannot fill up. */
        while (tud_cdc_n_available(interface_number) > 0) {
            (void)tud_cdc_n_read(interface_number, buffer, sizeof(buffer));
        }

        return;
    }

    if (rx_queue == NULL) {
        while (tud_cdc_n_available(MORSE_CDC_INTERFACE) > 0) {
            (void)tud_cdc_n_read(MORSE_CDC_INTERFACE, buffer, sizeof(buffer));
        }

        return;
    }

    while (tud_cdc_n_available(MORSE_CDC_INTERFACE) > 0) {
        uint32_t count = tud_cdc_n_read(
            MORSE_CDC_INTERFACE,
            buffer,
            sizeof(buffer)
        );

        if (count == 0) {
            break;
        }

        for (uint32_t i = 0; i < count; ++i) {
            /* Never wait in the USB callback. */
            (void)xQueueSend(rx_queue, &buffer[i], 0);
        }
    }
}
