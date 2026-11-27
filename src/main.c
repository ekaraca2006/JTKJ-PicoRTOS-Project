#include <stdbool.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "tusb.h"
#include "usbSerialDebug/helper.h"

#include "tkjhat/sdk.h"
#include "communication.h"
#include "display.h"
#include "morse.h"

/*
 * JTKJ Pico W Morse communication project - Tier 2
 * Group members: Emirhan and Shile
 *
 * We worked on the project together. For the final meeting, our main focus
 * areas are divided as follows (the work was not completely separate):
 * - Shile: USB communication (communication.c/.h), OLED and buzzer.
 * - Emirhan: IMU input (morse.c/.h), input state machine and task setup.
 * - Both: planning, integration, debugging and hardware testing.
 *
 * We propose an equal contribution split: 3 points for each member.
 */

#define USB_TASK_STACK_SIZE           2048
#define MORSE_TASK_STACK_SIZE         2048
#define COMMUNICATION_TASK_STACK_SIZE 1024
#define DISPLAY_TASK_STACK_SIZE       2048
#define BUZZER_TASK_STACK_SIZE        1024

#define USB_TASK_PRIORITY              3
#define APPLICATION_TASK_PRIORITY      2

static void fatal_error(void)
{
    /* This is used before the scheduler starts. */
    set_red_led_status(false);

    for (;;) {
        toggle_red_led();
        sleep_ms(200);
    }
}

static void usb_task(void *argument)
{
    (void)argument;

    for (;;) {
        /* The course examples run TinyUSB in its own FreeRTOS task. */
        tud_task();
    }
}

int main(void)
{
    /* Start the HAT and its I2C bus. */
    init_hat_sdk();
    sleep_ms(300);

    init_red_led();
    set_red_led_status(false);
    init_button1();

    if (!communication_init()) {
        fatal_error();
    }

    /* The OLED and buzzer are required for the Tier 2 receive interface. */
    if (!display_init()) {
        fatal_error();
    }

    /* The IMU and OLED use the same I2C bus. */
    SemaphoreHandle_t i2c_mutex = xSemaphoreCreateMutex();
    if (i2c_mutex == NULL) {
        fatal_error();
    }

    TaskHandle_t usb_handle = NULL;

    if (xTaskCreate(usb_task,
                    "usb",
                    USB_TASK_STACK_SIZE,
                    NULL,
                    USB_TASK_PRIORITY,
                    &usb_handle) != pdPASS) {
        fatal_error();
    }

    if (xTaskCreate(morse_task,
                    "morse_input",
                    MORSE_TASK_STACK_SIZE,
                    (void *)i2c_mutex,
                    APPLICATION_TASK_PRIORITY,
                    NULL) != pdPASS) {
        fatal_error();
    }

    if (xTaskCreate(communication_task,
                    "communication",
                    COMMUNICATION_TASK_STACK_SIZE,
                    NULL,
                    APPLICATION_TASK_PRIORITY,
                    NULL) != pdPASS) {
        fatal_error();
    }

    if (xTaskCreate(display_task,
                    "display_receive",
                    DISPLAY_TASK_STACK_SIZE,
                    (void *)i2c_mutex,
                    APPLICATION_TASK_PRIORITY,
                    NULL) != pdPASS) {
        fatal_error();
    }

    if (xTaskCreate(display_buzzer_task,
                    "buzzer",
                    BUZZER_TASK_STACK_SIZE,
                    NULL,
                    APPLICATION_TASK_PRIORITY,
                    NULL) != pdPASS) {
        fatal_error();
    }

#if (configNUMBER_OF_CORES > 1)
    /* Keep USB processing on core 0, like the course examples. */
    vTaskCoreAffinitySet(usb_handle, 1u << 0);
#endif

    /*
     * Keep these calls immediately before the scheduler.
     * This is the same startup order used by the working course examples.
     */
    tusb_init();
    (void)usb_serial_init();

    vTaskStartScheduler();

    /* The scheduler should not return. */
    fatal_error();
    return 0;
}
