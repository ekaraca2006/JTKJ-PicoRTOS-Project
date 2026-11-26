#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "tkjhat/sdk.h"
#include "tkjhat/ssd1306.h"

#include "communication.h"
#include "display.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define LINE_LENGTH 20
#define MESSAGE_LENGTH 40
#define BUZZER_QUEUE_SIZE 512

static ssd1306_t oled;
static QueueHandle_t buzzer_queue = NULL;

enum receive_state {
    RX_IDLE,
    RX_RECEIVING,
    RX_COMPLETE
};

bool display_init(void)
{
    oled.external_vcc = false;

    if (!ssd1306_init(&oled, SCREEN_WIDTH, SCREEN_HEIGHT,
                      SSD1306_I2C_ADDRESS, i2c_default)) {
        return false;
    }

    ssd1306_poweron(&oled);
    ssd1306_clear(&oled);
    ssd1306_draw_string(&oled, 0, 0, 1, "MORSE READY");
    ssd1306_draw_string(&oled, 0, 16, 1, "Tilt one way: .");
    ssd1306_draw_string(&oled, 0, 28, 1, "Tilt other way: -");
    ssd1306_draw_string(&oled, 0, 48, 1, "SW1 sends");
    ssd1306_show(&oled);

    init_buzzer();

    /* The buzzer has its own task so it cannot slow down USB reception. */
    buzzer_queue = xQueueCreate(BUZZER_QUEUE_SIZE, sizeof(char));
    if (buzzer_queue == NULL) {
        return false;
    }

    return true;
}

/* Draw the most recent 40 received characters on the OLED. */
static void update_display(const char *message,
                           size_t message_length,
                           enum receive_state state,
                           SemaphoreHandle_t i2c_mutex)
{
    char first_line[LINE_LENGTH + 1];
    char second_line[LINE_LENGTH + 1];
    size_t first_length = message_length;
    size_t second_length;

    if (first_length > LINE_LENGTH) {
        first_length = LINE_LENGTH;
    }

    second_length = message_length - first_length;
    if (second_length > LINE_LENGTH) {
        second_length = LINE_LENGTH;
    }

    memcpy(first_line, message, first_length);
    first_line[first_length] = '\0';

    memcpy(second_line, message + first_length, second_length);
    second_line[second_length] = '\0';

    xSemaphoreTake(i2c_mutex, portMAX_DELAY);

    ssd1306_clear(&oled);

    if (state == RX_IDLE && message_length == 0) {
        ssd1306_draw_string(&oled, 0, 0, 1, "MORSE READY");
        ssd1306_draw_string(&oled, 0, 16, 1, "Tilt one way: .");
        ssd1306_draw_string(&oled, 0, 28, 1, "Tilt other way: -");
        ssd1306_draw_string(&oled, 0, 48, 1, "SW1 sends");
    } else {
        ssd1306_draw_string(&oled, 0, 0, 1, "MORSE RECEIVED");
        ssd1306_draw_string(&oled, 0, 14, 1, first_line);
        ssd1306_draw_string(&oled, 0, 26, 1, second_line);

        if (state == RX_COMPLETE) {
            ssd1306_draw_string(&oled, 0, 48, 1, "RX COMPLETE");
        } else {
            ssd1306_draw_string(&oled, 0, 48, 1, "RECEIVING...");
        }
    }

    ssd1306_show(&oled);
    xSemaphoreGive(i2c_mutex);
}

/* Add a new character to the end of the screen buffer. */
static void add_to_message(char *message,
                           size_t *message_length,
                           char character)
{
    if (*message_length == MESSAGE_LENGTH) {
        memmove(message, message + 1, MESSAGE_LENGTH - 1);
        (*message_length)--;
    }

    message[*message_length] = character;
    (*message_length)++;
    message[*message_length] = '\0';
}

void display_task(void *argument)
{
    SemaphoreHandle_t i2c_mutex = (SemaphoreHandle_t)argument;
    char message[MESSAGE_LENGTH + 1] = {0};
    size_t message_length = 0;
    enum receive_state state = RX_IDLE;
    TickType_t last_display_update = 0;
    bool display_needs_update = true;

    update_display(message, message_length, state, i2c_mutex);
    last_display_update = xTaskGetTickCount();

    for (;;) {
        char character;

        /* Wait briefly for data, then process all characters already waiting. */
        if (communication_receive(&character, pdMS_TO_TICKS(20))) {
            do {
                if (character == '\r') {
                    /* Ignore the carriage return in Windows-style line endings. */
                } else if (character == '\n') {
                    state = RX_COMPLETE;
                    display_needs_update = true;
                    (void)xQueueSend(buzzer_queue, &character, 0);
                } else {
                    if (state == RX_COMPLETE) {
                        message_length = 0;
                        message[0] = '\0';
                        state = RX_IDLE;
                    }

                    add_to_message(message, &message_length, character);
                    state = RX_RECEIVING;
                    display_needs_update = true;

                    if (character == '.' || character == '-' || character == ' ') {
                        /* The buzzer task handles this separately. */
                        (void)xQueueSend(buzzer_queue, &character, 0);
                    }
                }
            } while (communication_receive(&character, 0));
        }

        /* Do not redraw the OLED for every byte. That would slow USB reception. */
        if (display_needs_update &&
            (state == RX_COMPLETE ||
             xTaskGetTickCount() - last_display_update >= pdMS_TO_TICKS(200))) {
            update_display(message, message_length, state, i2c_mutex);
            last_display_update = xTaskGetTickCount();
            display_needs_update = false;
        }
    }
}

void buzzer_task(void *argument)
{
    char character;

    (void)argument;

    for (;;) {
        if (xQueueReceive(buzzer_queue, &character, portMAX_DELAY) == pdPASS) {
            if (character == '.') {
                buzzer_play_tone(1000, 70);
                vTaskDelay(pdMS_TO_TICKS(40));
            } else if (character == '-') {
                buzzer_play_tone(1000, 210);
                vTaskDelay(pdMS_TO_TICKS(40));
            } else if (character == ' ') {
                /* Two spaces from the workstation make a longer word pause. */
                vTaskDelay(pdMS_TO_TICKS(120));
            } else if (character == '\n') {
                buzzer_play_tone(1400, 80);
                vTaskDelay(pdMS_TO_TICKS(60));
                buzzer_play_tone(1800, 80);
            }
        }
    }
}
