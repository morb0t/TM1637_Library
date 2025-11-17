/*
 * tm1637.c
 *
 *  Created on: Nov 15, 2025
 *      Author: Anoaur ELHARDA
 */

#include "main.h"
#include "tm1637.h"

// ===================================================================================
// USER CONFIGURATION (MANDATORY: REPLACE WITH YOUR CUBEMX-GENERATED PIN DEFINITIONS)
// ===================================================================================
// Example based on PA4/PA5
#define TM1637_CLK_GPIO_Port GPIOA
#define TM1637_CLK_Pin GPIO_PIN_4
#define TM1637_DIO_GPIO_Port GPIOA
#define TM1637_DIO_Pin GPIO_PIN_5

// ===================================================================================
// LOW-LEVEL PIN CONTROL MACROS
// ===================================================================================
#define CLK_LOW()  HAL_GPIO_WritePin(TM1637_CLK_GPIO_Port, TM1637_CLK_Pin, GPIO_PIN_RESET)
#define CLK_HIGH() HAL_GPIO_WritePin(TM1637_CLK_GPIO_Port, TM1637_CLK_Pin, GPIO_PIN_SET)
#define DIO_LOW()  HAL_GPIO_WritePin(TM1637_DIO_GPIO_Port, TM1637_DIO_Pin, GPIO_PIN_RESET)
#define DIO_HIGH() HAL_GPIO_WritePin(TM1637_DIO_GPIO_Port, TM1637_DIO_Pin, GPIO_PIN_SET)
#define DIO_READ() HAL_GPIO_ReadPin(TM1637_DIO_GPIO_Port, TM1637_DIO_Pin)

// --- Segment Codes (Common Cathode) ---
// The index 10 (0x00) is used for a blank digit
const uint8_t segment_codes[] = { 0x3F, // 0: 0b00111111
		0x06, // 1: 0b00000110
		0x5B, // 2: 0b01011011
		0x4F, // 3: 0b01001111
		0x66, // 4: 0b01100110
		0x6D, // 5: 0b01101101
		0x7D, // 6: 0b01111101
		0x07, // 7: 0b00000111
		0x7F, // 8: 0b01111111
		0x6F, // 9: 0b01101111
		0x00, // 10: BLANK
		};

// ===================================================================================
// HELPER FUNCTIONS (Timing and DIO Direction Switching)
// ===================================================================================

/**
 * @brief Provides a short delay for clock timing.
 */
static inline void TM1637_Delay(void) {
	// Using a tight loop or __NOP() provides the shortest possible delay.
	// Adjust '10' for slower/faster clocking if needed (250kHz max recommended CLK rate).
	for (volatile int i = 0; i < 10; i++)
		__NOP();
}

/**
 * @brief Configures the DIO pin as Input (required to read ACK).
 */
static void DIO_AsInput() {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = TM1637_DIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(TM1637_DIO_GPIO_Port, &GPIO_InitStruct);
}

/**
 * @brief Configures the DIO pin as Output Open-Drain (required to drive data).
 */
static void DIO_AsOutput() {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = TM1637_DIO_Pin;
	// NOTE: Must be OUTPUT_OD (Open-Drain) as configured in CubeMX
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(TM1637_DIO_GPIO_Port, &GPIO_InitStruct);
}

// ===================================================================================
// CORE PROTOCOL IMPLEMENTATION (Bit-Banging)
// ===================================================================================

/**
 * @brief Generates the TM1637 START condition.
 * CLK HIGH, DIO HIGH -> DIO LOW, then CLK LOW.
 */
static void TM1637_Start(void) {
	DIO_AsOutput();
	CLK_HIGH();
	DIO_HIGH();
	TM1637_Delay();
	DIO_LOW();
	TM1637_Delay();
	CLK_LOW();
	TM1637_Delay();
}

/**
 * @brief Generates the TM1637 STOP condition.
 * CLK LOW, DIO LOW -> CLK HIGH, DIO HIGH.
 */
static void TM1637_Stop(void) {
	DIO_AsOutput();
	CLK_LOW();
	DIO_LOW();
	TM1637_Delay();
	CLK_HIGH();
	TM1637_Delay();
	DIO_HIGH();
	TM1637_Delay();
}

/**
 * @brief Sends a single byte (LSB first) and waits for ACK.
 * @param data The byte to transmit.
 * @return 1 if ACK received, 0 otherwise.
 */
static uint8_t TM1637_SendByte(uint8_t data) {
	uint8_t i;
	uint8_t ack_status;

	// 1. Send 8 bits (LSB first)
	for (i = 0; i < 8; i++) {
		CLK_LOW();
		TM1637_Delay();

		// Output the bit (LSB is bit 0)
		if (data & 0x01) {
			DIO_HIGH();
		} else {
			DIO_LOW();
		}
		TM1637_Delay();

		CLK_HIGH();
		TM1637_Delay();
		data >>= 1; // Shift right for LSB first
	}

	// 2. Read ACK from TM1637 (9th clock cycle)
	CLK_LOW();
	TM1637_Delay();

	// Switch DIO to Input mode to read the ACK
	DIO_AsInput();
	TM1637_Delay();

	CLK_HIGH(); // Pulse clock to read ACK
	TM1637_Delay();

	// Check if the TM1637 pulled the line LOW (ACK = GPIO_PIN_RESET)
	ack_status = (DIO_READ() == GPIO_PIN_RESET);

	CLK_LOW();
	TM1637_Delay();

	// Switch DIO back to Output mode for next transmission
	DIO_AsOutput();

	return ack_status;
}

/**
 * @brief Helper function to write a sequence of segments to the display.
 * @param seg_data Pointer to an array of 4 segment bytes (D3, D2, D1, D0).
 * @param start_addr The starting address offset (usually 0).
 */
static void TM1637_WriteDisplayData(uint8_t seg_data[4], uint8_t start_addr) {
	// 1. Send Data Command (Auto Address Increment 0x40)
	TM1637_Start();
	TM1637_SendByte(ADDR_AUTO);
	TM1637_Stop();

	// 2. Send Address Command (Start at 0xC0) and 4 data bytes
	TM1637_Start();
	TM1637_SendByte(START_CMD | (start_addr & 0x03)); // Start address C0H, C1H, etc.

	for (int i = 0; i < 4; i++) {
		TM1637_SendByte(seg_data[i]);
	}
	TM1637_Stop();
}

// ===================================================================================
// PUBLIC API IMPLEMENTATION
// ===================================================================================

void TM1637_Init(void) {
	// Clear the display buffer
	uint8_t blank_data[4] = { segment_codes[10], segment_codes[10],
			segment_codes[10], segment_codes[10] };
	TM1637_WriteDisplayData(blank_data, 0);

	// Turn on display and set default brightness
	TM1637_SetBrightness(BRIGHTNESS_MAX);
}

void TM1637_SetBrightness(uint8_t brightness) {
	// Brightness level 0-7 is safe
	if (brightness > 7)
		brightness = 7;

	// Display Control Command is 0x88 | (brightness | display_on_flag)
	uint8_t ctrl_byte = 0x88 | brightness;

	TM1637_Start();
	TM1637_SendByte(ctrl_byte);
	TM1637_Stop();
}

void TM1637_DisplayDec(uint16_t val, bool useDot) {
	uint8_t digits[4];
	uint8_t segments_to_display[4];

	// Isolate the 4 digits (d3 is left-most, d0 is right-most)
	digits[0] = val % 10;
	digits[1] = (val / 10) % 10;
	digits[2] = (val / 100) % 10;
	digits[3] = (val / 1000) % 10;

	// Process digits from left to right (d3 to d0)
	uint8_t zero_state = 1; // 1 = currently suppressing leading zeros
	for (int i = 3; i >= 0; i--) {
		uint8_t digit_value = digits[i];

		if (zero_state && digit_value == 0 && i > 0) {
			// Suppress leading zero (use BLANK code)
			segments_to_display[3-i] = segment_codes[10];
		} else {
			// Found a non-zero digit or reached the last digit, stop suppression
			zero_state = 0;
			segments_to_display[3-i] = segment_codes[digit_value];
		}
	}

	// Apply the colon/dot flag.
	// The colon is typically the DP of the second digit (D2, which is index 2 in the array).
	if (useDot) {
		segments_to_display[2] |= 0x80; // 0x80 is the bitmask for the decimal point/colon
	}

	// Send the array to the display
	TM1637_WriteDisplayData(segments_to_display, 0);
}
