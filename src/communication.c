/*
 * USB communication for the Morse project.
 * Shile's part. Both group members worked on integration
 * and checked how the communication part works with the rest of the program.
 */
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "tusb.h"
#include "tkjhat/sdk.h"

#include "communication.h"

#define TX_QUEUE_SIZE 128
#define RX_QUEUE_SIZE 256
#define MORSE_CDC 1

/* CDC0 is used for debug and CDC1 is used for Morse messages. */
static QueueHandle_t tx_queue = NULL;
static QueueHandle_t rx_queue = NULL;

bool communication_init(void)
{
    tx_queue = xQueueCreate(TX_QUEUE_SIZE, sizeof(char));
    rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(char));

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

    return xQueueSend(tx_queue, &symbol, pdMS_TO_TICKS(100)) == pdPASS;
}

bool communication_receive(char *character, TickType_t wait_ticks)
{
    if (rx_queue == NULL || character == NULL) {
        return false;
    }

    return xQueueReceive(rx_queue, character, wait_ticks) == pdPASS;
}

/* Wait until the workstation is connected, then send one character. */
static void send_usb_character(char character)
{
    for (;;) {
        if (tud_mounted() && tud_cdc_n_connected(MORSE_CDC) &&
            tud_cdc_n_write_available(MORSE_CDC) > 0) {

            if (tud_cdc_n_write(MORSE_CDC, &character, 1) == 1) {
                tud_cdc_n_write_flush(MORSE_CDC);
                return;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

static void flash_led_after_message(void)
{
    int i;

    for (i = 0; i < 2; i++) {
        set_red_led_status(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        set_red_led_status(false);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void communication_task(void *argument)
{
    char symbol;

    (void)argument;

    for (;;) {
        if (xQueueReceive(tx_queue, &symbol, portMAX_DELAY) == pdPASS) {
            send_usb_character(symbol);

            if (symbol == '\n') {
                flash_led_after_message();
            }
        }
    }
}

/* TinyUSB calls this when data has arrived on a CDC interface. */
void tud_cdc_rx_cb(uint8_t interface_number)
{
    char buffer[32];
    uint32_t count;
    uint32_t i;

    /* Only CDC1 carries Morse messages. Read other interfaces to clear them. */
    while (tud_cdc_n_available(interface_number) > 0) {
        count = tud_cdc_n_read(interface_number, buffer, sizeof(buffer));
        if (count == 0) {
            break;
        }

        if (interface_number == MORSE_CDC && rx_queue != NULL) {
            for (i = 0; i < count; i++) {
                /* Do not block inside the USB callback. A full queue can drop data. */
                (void)xQueueSend(rx_queue, &buffer[i], 0);
            }
        }
    }
}
