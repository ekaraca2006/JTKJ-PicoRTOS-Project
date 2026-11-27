/*
 * Display and buzzer module.
 * Shile mainly focused on the OLED and buzzer part. 
 */
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

#define DISPLAY_WIDTH_PIXELS  128
#define DISPLAY_HEIGHT_PIXELS 64
#define DISPLAY_LINE_LENGTH   20
#define RX_MESSAGE_LENGTH     (DISPLAY_LINE_LENGTH * 2)

#define DOT_TONE_MS           100
#define DASH_TONE_MS          300
#define SYMBOL_GAP_MS         100
#define LETTER_GAP_MS         250

#define BUZZER_QUEUE_LENGTH   128

static ssd1306_t oled;
static bool oled_ready = false;
static QueueHandle_t buzzer_queue = NULL;

typedef enum {
    RX_IDLE = 0,
    RX_RECEIVING,
    RX_COMPLETE
} receive_state_t;

bool display_init(void)
{
    init_buzzer();
    buzzer_queue = xQueueCreate(BUZZER_QUEUE_LENGTH, sizeof(char));
    if (buzzer_queue == NULL) {
        return false;
    }

    oled.external_vcc = false;

    if (!ssd1306_init(&oled,
                      DISPLAY_WIDTH_PIXELS,
                      DISPLAY_HEIGHT_PIXELS,
                      SSD1306_I2C_ADDRESS,
                      i2c_default)) {
        vQueueDelete(buzzer_queue);
        buzzer_queue = NULL;
        return false;
    }

    ssd1306_poweron(&oled);
    ssd1306_clear(&oled);
    ssd1306_draw_string(&oled, 0, 0, 1, "MORSE READY");
    ssd1306_draw_string(&oled, 0, 16, 1, "Tilt + : dot");
    ssd1306_draw_string(&oled, 0, 28, 1, "Tilt - : dash");
    ssd1306_draw_string(&oled, 0, 48, 1, "SW1 sends");
    ssd1306_show(&oled);

    oled_ready = true;
    return true;
}

static void render_received_message(const char *message,
                                    size_t message_length,
                                    receive_state_t state,
                                    SemaphoreHandle_t i2c_mutex)
{
    if (!oled_ready) {
        return;
    }

    char first_line[DISPLAY_LINE_LENGTH + 1];
    char second_line[DISPLAY_LINE_LENGTH + 1];

    size_t first_length = message_length;
    if (first_length > DISPLAY_LINE_LENGTH) {
        first_length = DISPLAY_LINE_LENGTH;
    }

    size_t second_length = 0;
    if (message_length > DISPLAY_LINE_LENGTH) {
        second_length = message_length - DISPLAY_LINE_LENGTH;
        if (second_length > DISPLAY_LINE_LENGTH) {
            second_length = DISPLAY_LINE_LENGTH;
        }
    }

    memcpy(first_line, message, first_length);
    first_line[first_length] = '\0';

    memcpy(second_line, message + first_length, second_length);
    second_line[second_length] = '\0';

    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
        ssd1306_clear(&oled);

        if (state == RX_IDLE && message_length == 0) {
            ssd1306_draw_string(&oled, 0, 0, 1, "MORSE READY");
            ssd1306_draw_string(&oled, 0, 16, 1, "Tilt + : dot");
            ssd1306_draw_string(&oled, 0, 28, 1, "Tilt - : dash");
            ssd1306_draw_string(&oled, 0, 48, 1, "SW1 sends");
        } else {
            ssd1306_draw_string(&oled, 0, 0, 1, "MORSE RX");
            ssd1306_draw_string(&oled, 0, 14, 1, first_line);
            ssd1306_draw_string(&oled, 0, 26, 1, second_line);

            if (state == RX_COMPLETE) {
                ssd1306_draw_string(&oled, 0, 46, 1, "RX COMPLETE");
            } else if (state == RX_RECEIVING) {
                ssd1306_draw_string(&oled, 0, 46, 1, "RECEIVING...");
            } else {
                ssd1306_draw_string(&oled, 0, 46, 1, "WAITING");
            }
        }

        ssd1306_show(&oled);
        xSemaphoreGive(i2c_mutex);
    }
}

static void queue_buzzer_event(char symbol)
{
    if (buzzer_queue != NULL) {
        /* Do not wait here. The display task should keep reading USB data. */
        (void)xQueueSend(buzzer_queue, &symbol, 0);
    }
}

void display_buzzer_task(void *argument)
{
    (void)argument;

    char symbol;

    for (;;) {
        if (buzzer_queue == NULL) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (xQueueReceive(buzzer_queue, &symbol, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if (symbol == '.') {
            buzzer_play_tone(1000, DOT_TONE_MS);
            buzzer_turn_off();
            vTaskDelay(pdMS_TO_TICKS(SYMBOL_GAP_MS));
        } else if (symbol == '-') {
            buzzer_play_tone(1000, DASH_TONE_MS);
            buzzer_turn_off();
            vTaskDelay(pdMS_TO_TICKS(SYMBOL_GAP_MS));
        } else if (symbol == ' ') {
            vTaskDelay(pdMS_TO_TICKS(LETTER_GAP_MS));
        } else if (symbol == '\n') {
            buzzer_play_tone(1400, 80);
            buzzer_turn_off();
            vTaskDelay(pdMS_TO_TICKS(60));
            buzzer_play_tone(1800, 80);
            buzzer_turn_off();
        }
    }
}

void display_task(void *argument)
{
    SemaphoreHandle_t i2c_mutex = (SemaphoreHandle_t)argument;

    char message[RX_MESSAGE_LENGTH + 1] = {0};
    size_t message_length = 0;
    receive_state_t state = RX_IDLE;

    render_received_message(message, message_length, state, i2c_mutex);

    for (;;) {
        char received_character;

        if (!communication_receive(&received_character, pdMS_TO_TICKS(100))) {
            continue;
        }

        /* Serial clients commonly send CRLF. Only LF ends the message. */
        if (received_character == '\r') {
            continue;
        }

        if (received_character == '\n') {
            state = RX_COMPLETE;
            render_received_message(message, message_length, state, i2c_mutex);
            queue_buzzer_event(received_character);
            continue;
        }

        if (state == RX_COMPLETE) {
            message_length = 0;
            message[0] = '\0';
            state = RX_IDLE;
        }

        if (message_length >= RX_MESSAGE_LENGTH) {
            memmove(message, message + 1, RX_MESSAGE_LENGTH - 1);
            message_length = RX_MESSAGE_LENGTH - 1;
        }

        message[message_length++] = received_character;
        message[message_length] = '\0';
        state = RX_RECEIVING;

        render_received_message(message, message_length, state, i2c_mutex);
        queue_buzzer_event(received_character);
    }
}
