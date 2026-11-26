#include <stdbool.h>
#include <stddef.h>

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

#define USB_STACK_SIZE           2048
#define MORSE_STACK_SIZE         2048
#define COMMUNICATION_STACK_SIZE 1024
#define DISPLAY_STACK_SIZE       2048
#define BUZZER_STACK_SIZE        1024

#define USB_PRIORITY             3
#define APP_PRIORITY             2

/* Blink the LED forever if the program cannot finish setting up. */
static void show_error(void)
{
    for (;;) {
        toggle_red_led();
        sleep_ms(200);
    }
}

/* TinyUSB must be checked regularly while the device is running. */
static void usb_task(void *argument)
{
    (void)argument;

    for (;;) {
        tud_task();
    }
}

int main(void)
{
    SemaphoreHandle_t i2c_mutex;
    TaskHandle_t usb_task_handle = NULL;

    /* Start the HAT and its I2C bus. */
    init_hat_sdk();
    sleep_ms(300);

    init_red_led();
    set_red_led_status(false);
    init_button1();

    /* Set up the OLED, buzzer, IMU, and message queues. */
    if (!display_init()) {
        show_error();
    }

    if (!morse_init()) {
        show_error();
    }

    if (!communication_init()) {
        show_error();
    }

    /* The IMU and OLED both use I2C, so they share this mutex. */
    i2c_mutex = xSemaphoreCreateMutex();
    if (i2c_mutex == NULL) {
        show_error();
    }

    /* Start the tasks. */
    if (xTaskCreate(usb_task, "usb", USB_STACK_SIZE, NULL,
                    USB_PRIORITY, &usb_task_handle) != pdPASS) {
        show_error();
    }

    if (xTaskCreate(morse_task, "morse", MORSE_STACK_SIZE, i2c_mutex,
                    APP_PRIORITY, NULL) != pdPASS) {
        show_error();
    }

    if (xTaskCreate(communication_task, "communication",
                    COMMUNICATION_STACK_SIZE, NULL,
                    APP_PRIORITY, NULL) != pdPASS) {
        show_error();
    }

    if (xTaskCreate(display_task, "display", DISPLAY_STACK_SIZE,
                    i2c_mutex, APP_PRIORITY, NULL) != pdPASS) {
        show_error();
    }

    if (xTaskCreate(buzzer_task, "buzzer", BUZZER_STACK_SIZE,
                    NULL, APP_PRIORITY, NULL) != pdPASS) {
        show_error();
    }

#if (configNUMBER_OF_CORES > 1)
    /* Keep USB processing on core 0, like the course examples. */
    vTaskCoreAffinitySet(usb_task_handle, 1u << 0);
#endif

    /* Initialize USB just before starting FreeRTOS. */
    tusb_init();
    (void)usb_serial_init();

    vTaskStartScheduler();

    /* The scheduler should not return. */
    show_error();
    return 0;
}
