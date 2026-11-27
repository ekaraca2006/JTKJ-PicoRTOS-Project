# JTKJ Pico W Morse project - Tier 2

Group members: Emirhan and Shile

## What the program does

The Raspberry Pi Pico W and JTKJ HAT are used to send and receive Morse symbols. The program is written in C and uses FreeRTOS.

The device sends dots and dashes when it is tilted. Messages are sent to the course Serial Client over USB. The device can also receive Morse symbols from the Serial Client. Received symbols are shown on the OLED and played with the buzzer.

The program sends Morse characters as `.`, `-` and spaces. A message ends with two spaces and a newline (`\n`). It does not translate the symbols into letters.

## Sending a message

1. Keep the HAT flat after startup until it is ready.
2. Tilt it in the positive direction and hold it for about 150 ms to send a dot.
3. Return it to the flat position.
4. Tilt it in the opposite direction and hold it for about 150 ms to send a dash.
5. Return it to flat before sending the next symbol.
6. Pause between symbols. A longer pause adds the word separator.
7. Press SW1 to finish the message.

The program checks the stronger of the X and Y accelerometer values. A positive value above the threshold is treated as a dot, and a negative value below the threshold is treated as a dash. The exact physical direction depends on how the board is held.

The current settings are near the top of `src/morse.c`:

- Sampling period: 20 ms.
- Tilt threshold: 0.55 g.
- Release threshold: 0.35 g.
- Neutral range: between -0.25 g and +0.25 g.
- New-sample weight in the filter: 0.35.
- Gesture hold time: 150 ms.
- Neutral start time: 250 ms.
- Release stable time: 120 ms.
- Letter gap: 900 ms.
- Word gap: 2100 ms.

The filter is a simple exponential moving average. It combines the newest reading with the previous filtered value to reduce small changes in the sensor readings.

## Receiving a message

The Serial Client sends Morse characters to USB CDC1. The program puts the received characters in a FreeRTOS queue. The display task reads the queue and shows the latest characters on the OLED. The buzzer task plays a short tone for a dot and a longer tone for a dash. A newline ends the received message and plays two short confirmation tones.

The display keeps the latest 40 received characters. It ignores carriage return (`\r`) and uses line feed (`\n`) as the end of a message.

## FreeRTOS tasks

There are five tasks:

| Task | What it does |
|---|---|
| `usb_task` | Runs TinyUSB so the USB serial connection can be handled. |
| `morse_task` | Reads the IMU, filters the tilt value, runs the input state machine and adds symbols to the transmit queue. |
| `communication_task` | Takes symbols from the transmit queue and sends them through USB CDC1. |
| `display_task` | Reads received characters and updates the OLED. |
| `display_buzzer_task` | Plays sounds for received symbols. |

## State machines

The input state machine has five states: `INPUT_WAIT_FOR_NEUTRAL`, `INPUT_READY`, `INPUT_DOT_PENDING`, `INPUT_DASH_PENDING` and `INPUT_WAIT_FOR_RELEASE`.

The receiving side has three states: `RX_IDLE`, `RX_RECEIVING` and `RX_COMPLETE`.

The transitions are shown in `assets/state_machines.md`.

## Queues and I2C mutex

The program uses three queues:

- `tx_queue` holds symbols waiting to be sent to the workstation.
- `rx_queue` holds characters received from the workstation.
- `buzzer_queue` holds sound events for the buzzer task.

The IMU and OLED share the I2C bus. The `i2c_mutex` is passed to the IMU and display tasks so they do not use the bus at the same time.

The USB receive callback does not wait for space in the queue. This keeps the USB task responsive, but characters can be lost if the receive queue becomes full. The queue is sized for normal short messages.

## USB interfaces

- CDC0 is reserved for debug output.
- CDC1 is the Communication CDC interface used for Morse messages.

Use the course Serial Client and select the Communication CDC port.

## Main focus areas in the group

The project was developed and integrated together. To make preparation for the final meeting easier, the main areas each member should lead when explaining are:

- **Shile:** `communication.c/.h` (USB CDC, transmit/receive queues and callback) and `display.c/.h` (OLED and buzzer).
- **Emirhan:** `morse.c/.h` (IMU sampling, filtering and input state machine) and the task setup in `main.c`.
- **Both:** project planning, integration, debugging, compiling and hardware testing.

This is a primary-focus split, not a claim that the work was done separately. Both members should understand the overall program and be able to explain how the modules work together. The proposed contribution split is 3 points for each member, subject to agreement with the evaluator.

## Build

Build in the course Linux environment where the Pico SDK, compiler and FreeRTOS kernel are installed/configured. The CMake project expects the usual `PICO_SDK_PATH` and `FREERTOS_KERNEL_PATH` setup used by the course environment.

From the project root:

```bash
rm -rf build
cmake -S . -B build -DPICO_BOARD=pico_w
cmake --build build -j$(nproc)
```

The expected UF2 file is `build/hat_app.uf2`. Flash it to the Pico W in BOOTSEL mode.

## Testing notes

The group reported that the previous project version compiled successfully and that outgoing Morse sending was tested successfully. This revised package changes the communication module and uses the `include/` folder, so it must be rebuilt and tested again before submission.

The following checks should be performed on the actual HAT and course Serial Client:

- [ ] OLED shows `MORSE READY` after startup.
- [ ] Positive and negative tilts generate different symbols.
- [ ] SW1 ends a message with two spaces and a newline.
- [ ] The Serial Client receives and decodes an outgoing message.
- [ ] Incoming symbols appear on the OLED.
- [ ] The buzzer produces the dot, dash and message-end sounds.
- [ ] The red LED flashes twice after an outgoing message is written to USB.
- [ ] More than one message can be sent and received.

Do not mark a check as complete until it has been tested with the revised package.
