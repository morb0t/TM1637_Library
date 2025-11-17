# TM1637 Library for STM32

This library provides a simple interface to control TM1637-based 7-segment displays with STM32 microcontrollers.

## Features

*   Display numbers (integers and floating point)
*   Set brightness
*   Control individual segments

## Hardware

*   **TM1637 7-Segment Display Module:** A common and inexpensive display module.
*   **STM32 Microcontroller:** The library is designed for STM32, and the example is specifically for the STM32F411RE.

## Usage

1.  **Include the library:**
    Add `TM1637.h` and `TM1637.c` to your STM32 project.

2.  **Initialize the display:**
    Create an instance of the `TM1637` display object, specifying the GPIO pins for CLK and DIO.

    ```c
    #include "TM1637.h"

    // Define the GPIO pins for the display
    #define TM1637_CLK_PIN GPIO_PIN_0
    #define TM1637_CLK_PORT GPIOA
    #define TM1637_DIO_PIN GPIO_PIN_1
    #define TM1637_DIO_PORT GPIOA

    // Create a display object
    TM1637_t tm1637_display;
    ```

3.  **Configure the display in your application:**
    Initialize the display and set the desired brightness.

    ```c
    void main(void) {
      // ... system initialization ...

      // Initialize the display
      TM1637_Init(&tm1637_display, TM1637_CLK_PORT, TM1637_CLK_PIN, TM1637_DIO_PORT, TM1637_DIO_PIN);

      // Set the brightness (0-7)
      TM1637_SetBrightness(&tm1637_display, 7);

      // Display a number
      TM1637_DisplayDecimal(&tm1637_display, 1234, 0);

      while(1) {
        // ... your application code ...
      }
    }
    ```

## Example Project

An example project demonstrating the usage of this library is provided.

**Note:** The example project is built for the **STM32F411RE** microcontroller. You may need to adapt the pin definitions and project settings for other STM32 models.

## Contributing

Contributions are welcome! Please feel free to submit a pull request or create an issue for any bugs or feature requests.
