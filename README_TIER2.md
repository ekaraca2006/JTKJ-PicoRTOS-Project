# JTKJ Pico W Morse project - Tier 2

This project uses the Raspberry Pi Pico W, JTKJ HAT, C, FreeRTOS, USB serial, the IMU, the OLED and the buzzer.

## Sending a message

1. Keep the HAT flat for about a quarter of a second after startup.
2. Tilt the HAT in one direction and hold it for about 150 ms. This sends a dot.
3. Return the HAT to flat.
4. Tilt it in the opposite direction and hold it for about 150 ms. This sends a dash.
5. Return it to flat before entering the next symbol.
6. Pause for about 0.9 seconds between letters.
7. Pause for about 2.1 seconds between words.
8. Press SW1 to finish and send the message.

The stronger of the IMU X and Y readings is used. Positive tilt sends a dot and negative tilt sends a dash. The physical direction depends on how the HAT is held.

The course Serial Client must be running and connected to the Communication CDC port. The final message ends with two spaces and a newline.

The red LED flashes twice after the bytes have been written to USB. This confirms local transmission, not that the workstation decoded the message.

## Receiving a message

The Serial Client sends Morse symbols to the Pico. The program shows the received symbols on the OLED and uses the buzzer to represent dots, dashes and message completion.

The program displays the most recent 40 received characters. It does not translate Morse into text because decoding is not required for this project tier.

## Tasks

- usb_task runs TinyUSB.
- morse_task reads the IMU and creates dots and dashes.
- communication_task sends queued characters through CDC1.
- display_task shows characters received from CDC1.
- display_buzzer_task plays the sounds separately so that a long tone does not stop display_task from reading new USB characters.

The IMU and OLED share the I2C bus. A FreeRTOS mutex prevents both tasks from using it at the same time.

## Important startup detail

The working course example initializes TinyUSB before the scheduler and initializes the IMU inside its own task. This project follows the same order.

The project only uses the accelerometer. It does not start the gyroscope because it is not needed for tilt detection. If IMU initialization fails, the Morse task waits one second and tries again. It does not stop USB from starting.

## Build

From the repository root:

    rm -rf build
    cmake -S . -B build -DPICO_BOARD=pico_w
    cmake --build build -j$(nproc)

The expected UF2 file is:

    build/hat_app.uf2

Flash the UF2 to the Pico W in BOOTSEL mode.

## USB interfaces

- CDC0 is reserved for debug output.
- CDC1 is the Communication CDC interface used for Morse messages.

The course Serial Client should use the JTKJ HAT configuration and the Communication CDC interface.

## Tuning the tilt input

The main values are near the top of src/morse.c:

- TILT_THRESHOLD_G controls how far the HAT must be tilted.
- TILT_RELEASE_THRESHOLD_G controls when a tilt is considered released.
- NEUTRAL_THRESHOLD_G controls what counts as flat.
- GESTURE_HOLD_MS controls how long a tilt must be held.

Change one value at a time and test again.

## Testing checklist

1. Build and flash the firmware.
2. Confirm the OLED shows MORSE READY.
3. Confirm the HAT can stay flat without creating symbols.
4. Send one dot and check the Serial Client.
5. Send one dash and check the Serial Client.
6. Send SOS and check that it is decoded.
7. Type SOS in the Serial Client and check the OLED and buzzer.
8. Press SW1 and confirm the red LED flashes after sending.

The thresholds and physical tilt directions still need to be checked on the actual HAT.

## Course work

Use the code as a reference that you study, test and adapt with your teammate. Both group members should understand the parts they present and follow the course rules for acknowledging outside or AI assistance.
