# JTKJ Pico W Morse project - Tier 2 version

This version is based on the files and SDK examples in the project repository. It uses C, FreeRTOS, the JTKJ HAT, the IMU, USB serial, the OLED display, and the buzzer.

## What the program does

### Sending a message

1. Keep the HAT flat for about a quarter of a second after starting.
2. Tilt it in one direction and hold it for about 150 ms. This sends a dot.
3. Return it to the flat position.
4. Tilt it in the opposite direction and hold it for about 150 ms. This sends a dash.
5. Return it to flat before entering the next symbol.
6. Pause for about 0.9 seconds to separate letters.
7. Pause for about 2.1 seconds to separate words.
8. Press SW1 to finish the message.

The program uses the stronger of the IMU X and Y readings. Positive tilt is a dot and negative tilt is a dash. The physical direction depends on how the HAT is held, so test it and adjust the thresholds in `morse.c` if needed.

The workstation must have the course Serial Client open and connected to the device's **Communication CDC** port. The project sends raw Morse characters, spaces, and a final newline. The final two spaces and newline are needed by the course Serial Client.

The red LED flashes twice after the complete message has been written and flushed through the USB CDC interface. This is local USB-send feedback. The course Serial Client does not send a separate acknowledgement back to the Pico, so the LED cannot prove that the workstation decoded the message.

### Receiving a message

The course Serial Client sends Morse characters to the Pico over **Communication CDC**. The program:

- Displays the received Morse characters on the OLED.
- Keeps the most recent 40 characters visible.
- Plays a short beep for a dot and a longer beep for a dash.
- Uses pauses for spaces and plays two tones when a message ends.

The program does not translate Morse into letters. The Tier 2 assignment says that decoding is not required.

## Files

Copy the files from `src/` into the matching `src/` folder in your project. Replace the root `CMakeLists.txt` with the one included here.

The code is split into four parts:

- `main.c` starts the HAT, creates the queues and mutex, and starts the FreeRTOS tasks.
- `morse.c` reads the IMU and recognizes the two tilting positions.
- `communication.c` sends and receives bytes over USB CDC1.
- `display.c` shows incoming symbols on the OLED and sends sound events to the buzzer task.

## Build

From the root of the repository:

```bash
rm -rf build
cmake -S . -B build -DPICO_BOARD=pico_w
cmake --build build -j$(nproc)
```

The build should create `build/hat_app.uf2`. Flash it to the Pico W using BOOTSEL mode.

## USB ports

The project has two USB CDC ports:

- **CDC0 / Stdout CDC** is for debug output.
- **CDC1 / Communication CDC** is for sending and receiving Morse messages.

Use the course Serial Client with the device configuration for the JTKJ HAT. Do not assume which `/dev/ttyACM*` name belongs to CDC1. Check the available ports.

## Values you may need to tune

In `morse.c`:

- `TILT_LIMIT` controls how far the HAT must be tilted.
- `RELEASE_LIMIT` controls when a tilt is no longer held.
- `NEUTRAL_LIMIT` controls what counts as flat.
- `HOLD_TIME_MS` controls how long a tilt must be held.

Change one value at a time and test again.

## Important testing notes

The buzzer runs in its own task. This means a long beep does not stop the program from reading incoming USB characters or updating the display. The buzzer has a queue for pending sounds. Extremely long messages can still exceed a finite queue, but normal short course demo messages should fit.

The IMU and OLED use the same I2C bus. A FreeRTOS mutex prevents the two tasks from using that bus at the same time.

## Before the course demonstration

Test these separately:

1. Build and flash the firmware.
2. Check that the OLED shows `MORSE READY`.
3. Confirm that the HAT stays ready when it is flat.
4. Test a dot and a dash separately and check the outgoing Serial Client display.
5. Send a short message and check that the Serial Client decodes it.
6. Type a short message in the Serial Client and check that the raw Morse characters appear on the OLED.
7. Confirm that the buzzer plays dots and dashes differently.
8. Confirm that the red LED flashes after sending.
9. Repeat the test several times to check that no characters are missed.

## What is not included

This is a USB-based Tier 2 implementation. It does not include Tier 3 communication with another group's device or changes to the TKJHAT SDK.

## Course work and group contribution

Use this code as a reference implementation that you study, test, and adapt with your teammate. The course asks every group member to take part in development and to explain the implementation and their own contribution at the final meeting. Check the course's rules for acknowledging external and AI assistance before submitting. Do not claim code you have not written or understood as your own work.

The code still needs to be built with your installed Pico SDK and tested with the actual HAT. The IMU thresholds and tilt directions must be confirmed on the real hardware.
