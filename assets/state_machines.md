# State machines used in the project

## IMU input state machine

```text
                  neutral stable for 250 ms
  INPUT_WAIT_FOR_NEUTRAL ----------------------> INPUT_READY
                                                     |
                         +---------------------------+---------------------------+
                         |                                                           |
              tilt >= +0.55 g                                             tilt <= -0.55 g
                         |                                                           |
                         v                                                           v
                  INPUT_DOT_PENDING                                      INPUT_DASH_PENDING
                         |                                                           |
          held for 150 ms, still tilted                          held for 150 ms, still tilted
                         |                                                           |
                         +---------------------------+-------------------------------+
                                                     |
                                                     v
                                          INPUT_WAIT_FOR_RELEASE
                                                     |
                                      neutral stable for 120 ms
                                                     |
                                                     v
                                                INPUT_READY
```

If a pending tilt falls below its release threshold before the hold time is reached, the input returns to `INPUT_READY` without sending a symbol.

SW1 finishes the current message. The program adds enough spaces to make the required two spaces before the newline.

## Receive state machine

```text
RX_IDLE -- receive a Morse character --> RX_RECEIVING
RX_RECEIVING -- receive another character --> RX_RECEIVING
RX_RECEIVING -- receive LF (\n) --> RX_COMPLETE
RX_COMPLETE -- receive next normal character --> RX_RECEIVING
```

Carriage return (`\r`) is ignored. The received characters are displayed as Morse symbols and are not translated into letters.
