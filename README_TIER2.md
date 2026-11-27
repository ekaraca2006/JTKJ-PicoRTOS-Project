JTKJ Pico W Morse Communication Project – Tier 2

Group members:** Emirhan and Shile

1. Project Overview

This project uses a Raspberry Pi Pico W and JTKJ HAT to send and receive Morse code. The program is written in C and uses FreeRTOS.

Users can send Morse symbols by tilting the HAT. The symbols are sent to the course Serial Client through USB. The device can also receive Morse symbols from the Serial Client, display them on the OLED screen and play them through the buzzer.

The program handles dots (`.`), dashes (`-`) and spaces. It does not translate Morse code into normal letters.

2. Sending Morse Code

After startup, the HAT should be kept flat until the device is ready.

 Tilt in the positive direction and hold for around 150 ms to send a dot.
 Tilt in the opposite direction and hold for around 150 ms to send a dash.
 Return the HAT to the flat position before sending another symbol.
 Press SW1 to finish the message.

The program reads the X and Y accelerometer values and uses the axis with the stronger movement. A positive value above the threshold is detected as a dot, while a negative value below the threshold is detected as a dash.

An exponential moving average filter is used to make the sensor readings more stable and reduce small changes.

The main settings are defined in `src/morse.c`:

- Sampling period: 20 ms
- Tilt threshold: 0.55 g
- Release threshold: 0.35 g
- Neutral range: -0.25 g to +0.25 g
- Filter new-sample weight: 0.35
- Gesture hold time: 150 ms
- Neutral start time: 250 ms
- Release stable time: 120 ms
- Letter gap: 900 ms
- Word gap: 2100 ms

When SW1 is pressed, the program ends the message with two spaces followed by a newline (`\n`).

3. Receiving Morse Code

Incoming Morse symbols are received through USB CDC1 and stored in a FreeRTOS queue.

The display task reads the received characters and shows the latest 40 characters on the OLED. The buzzer plays a short tone for a dot and a longer tone for a dash. When a newline is received, the message is considered complete and the buzzer plays two short confirmation tones.

The program ignores carriage return (`\r`) and uses line feed (`\n`) to detect the end of a message.

4. FreeRTOS Tasks

The program uses five FreeRTOS tasks to handle different parts of the system:

- `usb_task`: Runs TinyUSB and handles USB communication.
- `morse_task`: Reads the IMU sensor, filters the values and detects tilting gestures.
- `communication_task`: Sends outgoing Morse symbols through USB CDC1.
- `display_task`: Receives incoming characters and updates the OLED.
- `display_buzzer_task`: Plays sounds for received Morse symbols.

5. State Machines, Queues and Synchronization

The project uses two state machines.

The input state machine in `morse.c` has five states: `INPUT_WAIT_FOR_NEUTRAL`, `INPUT_READY`, `INPUT_DOT_PENDING`, `INPUT_DASH_PENDING` and `INPUT_WAIT_FOR_RELEASE`. These states control how the device detects dots and dashes and waits for the HAT to return to its neutral position.

The receiving state machine in `display.c` has three states: `RX_IDLE`, `RX_RECEIVING` and `RX_COMPLETE`. It manages the process of receiving and displaying a message.

The program also uses three FreeRTOS queues:

- `tx_queue`: Stores outgoing symbols before they are sent.
- `rx_queue`: Stores characters received from the Serial Client.
- `buzzer_queue`: Stores sound events for the buzzer task.

The IMU and OLED share the I2C bus, so an `i2c_mutex` is used to prevent them from accessing it at the same time.

The USB receive callback uses a non-blocking queue operation to keep USB communication responsive. However, some characters may be lost if the receive queue becomes full.

The state machine transitions are described in `assets/state_machines.md`.

6. USB Communication

The project uses two USB CDC interfaces:

- CDC0 is reserved for debugging.
- CDC1 is used for Morse communication.

The course Serial Client should connect to the Communication CDC port (CDC1).

After an outgoing message is written to USB, the red LED flashes twice. This indicates that the data has been written to USB, but does not confirm that the Serial Client has received or decoded it.

7. Work Division

We worked together on planning, integration, debugging and testing. Our main responsibilities were divided as follows:

Shile:
- Mainly worked on `communication.c/.h`, including USB CDC communication, transmit and receive queues, and the USB receive callback.
- Mainly worked on `display.c/.h`, including OLED updates and buzzer output.

Emirhan:
- Mainly worked on `morse.c/.h`, including IMU readings, sensor filtering and the input state machine.
- Mainly worked on setting up the FreeRTOS tasks in `main.c`.

Both:
- Project planning and feature decisions.
- Connecting the different modules.
- Debugging and fixing integration problems.
- Compiling and testing the complete program.

This division describes our main areas of responsibility. We worked together throughout the project, and both members understand the overall program. We propose an equal contribution of 3 points for each member.
