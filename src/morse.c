#include <stdbool.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "tkjhat/sdk.h"

#include "communication.h"
#include "morse.h"

/* Change these values if the HAT is too sensitive or not sensitive enough. */
#define SAMPLE_TIME_MS       20
#define TILT_LIMIT           0.55f
#define RELEASE_LIMIT        0.35f
#define NEUTRAL_LIMIT        0.25f
#define HOLD_TIME_MS         150
#define NEUTRAL_TIME_MS      250
#define RELEASE_TIME_MS      120

/* Time without a new symbol before a letter or word space is sent. */
#define LETTER_PAUSE_MS      900
#define WORD_PAUSE_MS        2100

/* A small filter makes the accelerometer readings less jumpy. */
#define NEW_READING_WEIGHT   0.35f

static bool is_neutral(float tilt)
{
    return tilt > -NEUTRAL_LIMIT && tilt < NEUTRAL_LIMIT;
}

/* Use the X or Y axis, whichever has the larger reading. */
static float get_tilt(float ax, float ay)
{
    float abs_x = (ax < 0.0f) ? -ax : ax;
    float abs_y = (ay < 0.0f) ? -ay : ay;

    if (abs_x > abs_y) {
        return ax;
    }

    return ay;
}

bool morse_init(void)
{
    if (init_ICM42670() != 0) {
        return false;
    }

    if (ICM42670_start_with_default_values() != 0) {
        return false;
    }

    return true;
}

/* Send a space after a letter or a word when the user pauses. */
static void check_for_spaces(TickType_t now,
                             bool message_started,
                             TickType_t last_symbol_time,
                             unsigned int *spaces_sent)
{
    TickType_t time_since_symbol;

    if (!message_started) {
        return;
    }

    time_since_symbol = now - last_symbol_time;

    if (*spaces_sent == 0 &&
        time_since_symbol >= pdMS_TO_TICKS(LETTER_PAUSE_MS)) {
        if (morse_send(' ')) {
            *spaces_sent = 1;
        }
    }

    if (*spaces_sent == 1 &&
        time_since_symbol >= pdMS_TO_TICKS(WORD_PAUSE_MS)) {
        if (morse_send(' ')) {
            *spaces_sent = 2;
        }
    }
}

/* Add the final two spaces and newline when SW1 is pressed. */
static bool finish_message(unsigned int *spaces_sent)
{
    while (*spaces_sent < 2) {
        if (!morse_send(' ')) {
            return false;
        }
        (*spaces_sent)++;
    }

    return morse_send('\n');
}

void morse_task(void *argument)
{
    SemaphoreHandle_t i2c_mutex = (SemaphoreHandle_t)argument;

    /* These are the states used to recognize one tilt at a time. */
    enum input_state {
        WAIT_FOR_FLAT,
        READY_FOR_TILT,
        CHECK_DOT,
        CHECK_DASH,
        WAIT_FOR_RELEASE
    } state = WAIT_FOR_FLAT;

    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t state_start_time = 0;
    TickType_t neutral_start_time = 0;
    TickType_t last_symbol_time = 0;
    TickType_t last_button_time = 0;
    TickType_t sample_time = pdMS_TO_TICKS(SAMPLE_TIME_MS);

    bool neutral_timer_started = false;
    bool filter_started = false;
    bool message_started = false;
    bool button_was_pressed = false;

    unsigned int spaces_sent = 0;
    float filtered_tilt = 0.0f;

    for (;;) {
        float ax = 0.0f;
        float ay = 0.0f;
        float az = 0.0f;
        float gx = 0.0f;
        float gy = 0.0f;
        float gz = 0.0f;
        float temperature = 0.0f;
        int read_result;
        TickType_t now;

        /* Read the IMU. The display task uses the same I2C bus. */
        xSemaphoreTake(i2c_mutex, portMAX_DELAY);
        read_result = ICM42670_read_sensor_data(
            &ax, &ay, &az, &gx, &gy, &gz, &temperature
        );
        xSemaphoreGive(i2c_mutex);

        now = xTaskGetTickCount();

        if (read_result == 0) {
            float tilt = get_tilt(ax, ay);

            if (!filter_started) {
                filtered_tilt = tilt;
                filter_started = true;
            } else {
                filtered_tilt = (1.0f - NEW_READING_WEIGHT) * filtered_tilt
                              + NEW_READING_WEIGHT * tilt;
            }

            /* Check for letter and word pauses before reading another tilt. */
            if (state == READY_FOR_TILT) {
                check_for_spaces(now, message_started,
                                 last_symbol_time, &spaces_sent);
            }

            switch (state) {
                case WAIT_FOR_FLAT:
                    if (is_neutral(filtered_tilt)) {
                        if (!neutral_timer_started) {
                            neutral_start_time = now;
                            neutral_timer_started = true;
                        } else if (now - neutral_start_time >=
                                   pdMS_TO_TICKS(NEUTRAL_TIME_MS)) {
                            state = READY_FOR_TILT;
                            neutral_timer_started = false;
                        }
                    } else {
                        neutral_timer_started = false;
                    }
                    break;

                case READY_FOR_TILT:
                    if (filtered_tilt >= TILT_LIMIT) {
                        state = CHECK_DOT;
                        state_start_time = now;
                    } else if (filtered_tilt <= -TILT_LIMIT) {
                        state = CHECK_DASH;
                        state_start_time = now;
                    }
                    break;

                case CHECK_DOT:
                    if (filtered_tilt < RELEASE_LIMIT) {
                        state = READY_FOR_TILT;
                    } else if (now - state_start_time >=
                               pdMS_TO_TICKS(HOLD_TIME_MS)) {
                        if (morse_send('.')) {
                            message_started = true;
                            last_symbol_time = now;
                            spaces_sent = 0;
                            state = WAIT_FOR_RELEASE;
                            neutral_timer_started = false;
                        }
                    }
                    break;

                case CHECK_DASH:
                    if (filtered_tilt > -RELEASE_LIMIT) {
                        state = READY_FOR_TILT;
                    } else if (now - state_start_time >=
                               pdMS_TO_TICKS(HOLD_TIME_MS)) {
                        if (morse_send('-')) {
                            message_started = true;
                            last_symbol_time = now;
                            spaces_sent = 0;
                            state = WAIT_FOR_RELEASE;
                            neutral_timer_started = false;
                        }
                    }
                    break;

                case WAIT_FOR_RELEASE:
                    if (is_neutral(filtered_tilt)) {
                        if (!neutral_timer_started) {
                            neutral_start_time = now;
                            neutral_timer_started = true;
                        } else if (now - neutral_start_time >=
                                   pdMS_TO_TICKS(RELEASE_TIME_MS)) {
                            state = READY_FOR_TILT;
                            neutral_timer_started = false;
                            last_symbol_time = now;
                        }
                    } else {
                        neutral_timer_started = false;
                    }
                    break;

                default:
                    state = WAIT_FOR_FLAT;
                    neutral_timer_started = false;
                    break;
            }
        }

        /* Check SW1 even if an IMU read failed. */
        {
            bool button_pressed = gpio_get(BUTTON1);

            if (button_pressed && !button_was_pressed &&
                now - last_button_time >= pdMS_TO_TICKS(300)) {
                if (message_started && finish_message(&spaces_sent)) {
                    message_started = false;
                    spaces_sent = 0;
                    state = WAIT_FOR_FLAT;
                    neutral_timer_started = false;
                }

                last_button_time = now;
            }

            button_was_pressed = button_pressed;
        }

        vTaskDelayUntil(&last_wake_time, sample_time);
    }
}
