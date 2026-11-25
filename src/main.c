#include <stdio.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

#include <tusb.h>
#include "usbSerialDebug/helper.h"

#include "tkjhat/sdk.h"
#include "communication.h"

#define DEFAULT_STACK_SIZE 2048
#define USB_STACK_SIZE 2048


enum state { WAITING = 1, DATA_READY };
enum state programState = WAITING;

// Global variable for ambient light
uint32_t ambientLight;


// Button interrupt callback
static void btn_fxn(uint gpio, uint32_t eventMask) {
    toggle_red_led();
}


// Sensor task
static void sensor_task(void *arg) {
    (void)arg;

    init_veml6030();

    for (;;) {
        if (programState == WAITING) {
            ambientLight = veml6030_read_light();
            programState = DATA_READY;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


// USB task: run TinyUSB continuously
static void usbTask(void *arg) {
    (void)arg;

    while (1) {
        tud_task();
    }
}


int main(void) {

    // Initialize TKJ HAT
    init_hat_sdk();
    sleep_ms(300);

    // Initialize Morse communication queue
    communication_init();

    // Initialize LED and button
    init_red_led();
    init_button1();

    gpio_set_irq_enabled_with_callback(
        SW1_PIN,
        GPIO_IRQ_EDGE_RISE,
        true,
        &btn_fxn
    );


    // Task handles
    TaskHandle_t hSensorTask = NULL;
    TaskHandle_t hCommunicationTask = NULL;
    TaskHandle_t hUSB = NULL;

    BaseType_t result;


    // Create USB task
    result = xTaskCreate(
        usbTask,
        "usb",
        USB_STACK_SIZE,
        NULL,
        3,
        &hUSB
    );

    if (result != pdPASS) {
        return 0;
    }


    // Pin USB task to core 0 when using multiple cores
    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(hUSB, 1u << 0);
    #endif


    // Create sensor task
    result = xTaskCreate(
        sensor_task,
        "sensor",
        DEFAULT_STACK_SIZE,
        NULL,
        2,
        &hSensorTask
    );

    if (result != pdPASS) {
        return 0;
    }


    // Create communication task
    result = xTaskCreate(
        communication_task,
        "communication",
        DEFAULT_STACK_SIZE,
        NULL,
        2,
        &hCommunicationTask
    );

    if (result != pdPASS) {
        return 0;
    }


    // Initialize TinyUSB and USB Serial Debug
    // Keep these calls immediately before the scheduler.
    tusb_init();
    usb_serial_init();


    // Start FreeRTOS scheduler
    vTaskStartScheduler();

    return 0;
}