#include <stdbool.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "tkjhat/sdk.h"

#include "communication.h"
#include "morse.h"

/* These values control how the tilting gestures are detected. */
#define SAMPLE_PERIOD_MS          20
#define TILT_THRESHOLD_G          0.55f
#define TILT_RELEASE_THRESHOLD_G  0.35f
#define NEUTRAL_THRESHOLD_G       0.25f
#define FILTER_NEW_WEIGHT         0.35f
#define GESTURE_HOLD_MS           150
#define NEUTRAL_STABLE_MS         250
#define RELEASE_STABLE_MS         120

/* Time between symbols, letters and words. */
#define LETTER_GAP_MS             900
#define WORD_GAP_MS               2100

#define SW1_PIN_NUMBER            BUTTON1

typedef enum {
    INPUT_WAIT_FOR_NEUTRAL = 0,
    INPUT_READY,
    INPUT_DOT_PENDING,
    INPUT_DASH_PENDING,
    INPUT_WAIT_FOR_RELEASE
} input_state_t;

static bool is_neutral(float value)
{
    return value > -NEUTRAL_THRESHOLD_G && value < NEUTRAL_THRESHOLD_G;
}

static float choose_tilt_axis(float ax, float ay)
{
    float abs_ax = (ax < 0.0f) ? -ax : ax;
    float abs_ay = (ay < 0.0f) ? -ay : ay;

    if (abs_ax >= abs_ay) {
        return ax;
    }

    return ay;
}

/*
 * This project only needs the accelerometer to detect tilting.
 * The old code also started the gyroscope, even though it is not used here.
 * The IMU is initialized from morse_task, after USB has started.
 */
static bool morse_init(void)
{
    if (init_ICM42670() != 0) {
        return false;
    }

    if (ICM42670_enable_accel_gyro_ln_mode() != 0) {
        return false;
    }

    if (ICM42670_startAccel(ICM42670_ACCEL_ODR_DEFAULT,
                            ICM42670_ACCEL_FSR_DEFAULT) != 0) {
        return false;
    }

    return true;
}

static void try_send_symbol(char symbol,
                            TickType_t now,
                            bool *message_active,
                            TickType_t *last_symbol_time,
                            unsigned int *spaces_sent)
{
    if (morse_send(symbol)) {
        *message_active = true;
        *last_symbol_time = now;
        *spaces_sent = 0;
    }
}

static void update_morse_spacing(TickType_t now,
                                 bool message_active,
                                 TickType_t last_symbol_time,
                                 unsigned int *spaces_sent)
{
    if (!message_active) {
        return;
    }

    TickType_t elapsed = now - last_symbol_time;

    if (*spaces_sent == 0 && elapsed >= pdMS_TO_TICKS(LETTER_GAP_MS)) {
        if (morse_send(' ')) {
            *spaces_sent = 1;
        }
    }

    if (*spaces_sent == 1 && elapsed >= pdMS_TO_TICKS(WORD_GAP_MS)) {
        if (morse_send(' ')) {
            *spaces_sent = 2;
        }
    }
}

static void finish_outgoing_message(bool *message_active,
                                    unsigned int *spaces_sent)
{
    if (!*message_active) {
        return;
    }

    /* The Serial Client needs two spaces and a newline at the end. */
    while (*spaces_sent < 2) {
        if (!morse_send(' ')) {
            return;
        }

        ++(*spaces_sent);
    }

    if (morse_send('\n')) {
        *message_active = false;
        *spaces_sent = 0;
    }
}

void morse_task(void *argument)
{
    SemaphoreHandle_t i2c_mutex = (SemaphoreHandle_t)argument;
    bool sensor_ready = false;

    /*
     * Do not initialize the IMU in main().
     * USB is already running by the time this task starts.
     * If the sensor is not ready, try again instead of stopping the program.
     */
    while (!sensor_ready) {
        if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
            sensor_ready = morse_init();
            xSemaphoreGive(i2c_mutex);
        }

        if (!sensor_ready) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    input_state_t state = INPUT_WAIT_FOR_NEUTRAL;
    TickType_t state_started = xTaskGetTickCount();
    TickType_t neutral_started = 0;
    bool neutral_timer_active = false;

    TickType_t last_symbol_time = 0;
    unsigned int spaces_sent = 0;
    bool message_active = false;

    bool filter_initialized = false;
    float filtered_tilt = 0.0f;

    bool sw1_was_pressed = false;
    TickType_t last_button_press = 0;

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t sample_period = pdMS_TO_TICKS(SAMPLE_PERIOD_MS);

    for (;;) {
        float ax = 0.0f;
        float ay = 0.0f;
        float az = 0.0f;
        float gx = 0.0f;
        float gy = 0.0f;
        float gz = 0.0f;
        float temperature = 0.0f;

        if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
            int read_result = ICM42670_read_sensor_data(
                &ax, &ay, &az, &gx, &gy, &gz, &temperature
            );

            xSemaphoreGive(i2c_mutex);

            if (read_result == 0) {
                float raw_tilt = choose_tilt_axis(ax, ay);

                if (!filter_initialized) {
                    filtered_tilt = raw_tilt;
                    filter_initialized = true;
                } else {
                    filtered_tilt =
                        (1.0f - FILTER_NEW_WEIGHT) * filtered_tilt +
                        FILTER_NEW_WEIGHT * raw_tilt;
                }

                TickType_t now = xTaskGetTickCount();

                /* Add letter and word spaces before starting a new symbol. */
                if (state == INPUT_READY) {
                    update_morse_spacing(now, message_active,
                                         last_symbol_time, &spaces_sent);
                }

                switch (state) {
                    case INPUT_WAIT_FOR_NEUTRAL:
                        if (is_neutral(filtered_tilt)) {
                            if (!neutral_timer_active) {
                                neutral_started = now;
                                neutral_timer_active = true;
                            } else if ((now - neutral_started) >=
                                       pdMS_TO_TICKS(NEUTRAL_STABLE_MS)) {
                                state = INPUT_READY;
                                neutral_timer_active = false;
                            }
                        } else {
                            neutral_timer_active = false;
                        }
                        break;

                    case INPUT_READY:
                        if (filtered_tilt >= TILT_THRESHOLD_G) {
                            state = INPUT_DOT_PENDING;
                            state_started = now;
                        } else if (filtered_tilt <= -TILT_THRESHOLD_G) {
                            state = INPUT_DASH_PENDING;
                            state_started = now;
                        }
                        break;

                    case INPUT_DOT_PENDING:
                        if (filtered_tilt < TILT_RELEASE_THRESHOLD_G) {
                            state = INPUT_READY;
                        } else if ((now - state_started) >=
                                   pdMS_TO_TICKS(GESTURE_HOLD_MS)) {
                            try_send_symbol('.', now, &message_active,
                                            &last_symbol_time, &spaces_sent);
                            state = INPUT_WAIT_FOR_RELEASE;
                            neutral_timer_active = false;
                        }
                        break;

                    case INPUT_DASH_PENDING:
                        if (filtered_tilt > -TILT_RELEASE_THRESHOLD_G) {
                            state = INPUT_READY;
                        } else if ((now - state_started) >=
                                   pdMS_TO_TICKS(GESTURE_HOLD_MS)) {
                            try_send_symbol('-', now, &message_active,
                                            &last_symbol_time, &spaces_sent);
                            state = INPUT_WAIT_FOR_RELEASE;
                            neutral_timer_active = false;
                        }
                        break;

                    case INPUT_WAIT_FOR_RELEASE:
                        if (is_neutral(filtered_tilt)) {
                            if (!neutral_timer_active) {
                                neutral_started = now;
                                neutral_timer_active = true;
                            } else if ((now - neutral_started) >=
                                       pdMS_TO_TICKS(RELEASE_STABLE_MS)) {
                                state = INPUT_READY;
                                neutral_timer_active = false;

                                /* Start the pause timer after the HAT is flat. */
                                if (message_active) {
                                    last_symbol_time = now;
                                }
                            }
                        } else {
                            neutral_timer_active = false;
                        }
                        break;

                    default:
                        state = INPUT_WAIT_FOR_NEUTRAL;
                        neutral_timer_active = false;
                        break;
                }

                /* SW1 finishes and sends the current message. */
                bool sw1_pressed = gpio_get(SW1_PIN_NUMBER);

                if (sw1_pressed && !sw1_was_pressed &&
                    (now - last_button_press) >= pdMS_TO_TICKS(300)) {
                    bool had_message = message_active;

                    finish_outgoing_message(&message_active, &spaces_sent);

                    if (had_message && !message_active) {
                        state = INPUT_WAIT_FOR_NEUTRAL;
                        neutral_timer_active = false;
                    }

                    last_button_press = now;
                }

                sw1_was_pressed = sw1_pressed;
            }
        }

        vTaskDelayUntil(&last_wake_time, sample_period);
    }
}
