/*
 * tm1637.c
 *
 *  Created on: Nov 15, 2025
 *      Author: Anoaur ELHARDA
 */

#include "main.h"
#include "tm1637.h"

// ===================================================================================
// LOW-LEVEL PIN CONTROL MACROS
// ===================================================================================
#define CLK_LOW(TM1637_t* tm1637)  HAL_GPIO_WritePin(tm1637->clk_port, tm1637->clk_pin, GPIO_PIN_RESET)
#define CLK_HIGH(TM1637_t* tm1637) HAL_GPIO_WritePin(tm1637->clk_port, tm1637->clk_pin,GPIO_PIN_SET)
#define DIO_LOW(TM1637_t* tm1637)  HAL_GPIO_WritePin(tm1637->dio_port, tm1637->dio_pin,GPIO_PIN_RESET)
#define DIO_HIGH(TM1637_t* tm1637) HAL_GPIO_WritePin(tm1637->dio_port, tm1637->dio_pin, GPIO_PIN_SET)
#define DIO_READ(TM1637_t* tm1637) HAL_GPIO_ReadPin(tm1637->dio_port, tm1637->dio_pin)

// --- Segment Codes (Common Cathode) ---
// The index 10 (0x00) is used for a blank digit
const uint8_t segment_codes[] = { 
		0x3F, // 0: 0b00111111
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
static void DIO_AsInput(TM1637_t* tm1637) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = tm1637->dio_pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(tm1637->dio_port, &GPIO_InitStruct);
}

/**
 * @brief Configures the DIO pin as Output Open-Drain (required to drive data).
 */
static void DIO_AsOutput(TM1637_t* tm1637) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	GPIO_InitStruct.Pin = tm1637->dio_pin;
	// NOTE: Must be OUTPUT_OD (Open-Drain) as configured in CubeMX
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(tm1637->dio_port, &GPIO_InitStruct);
}

// ===================================================================================
// CORE PROTOCOL IMPLEMENTATION (Bit-Banging)
// ===================================================================================

/**
 * @brief Generates the TM1637 START condition.
 * CLK HIGH, DIO HIGH -> DIO LOW, then CLK LOW.
 */
static void TM1637_Start(TM1637_t* tm1637) {
	DIO_AsOutput(tm1637);
	CLK_HIGH(tm1637);
	DIO_HIGH(tm1637);
	TM1637_Delay();
	DIO_LOW(tm1637);
	TM1637_Delay();
	CLK_LOW(tm1637);
	TM1637_Delay();
}

/**
 * @brief Generates the TM1637 STOP condition.
 * CLK LOW, DIO LOW -> CLK HIGH, DIO HIGH.
 */
static void TM1637_Stop(TM1637_t* tm1637) {
	DIO_AsOutput(tm1637);
	CLK_LOW(tm1637);
	DIO_LOW(tm1637);
	TM1637_Delay();
	CLK_HIGH(tm1637);
	TM1637_Delay();
	DIO_HIGH(tm1637);
	TM1637_Delay();
}

/**
 * @brief Sends a single byte (LSB first) and waits for ACK.
 * @param data The byte to transmit.
 * @return 1 if ACK received, 0 otherwise.
 */
static uint8_t TM1637_SendByte(TM1637_t* tm1637, uint8_t data) {
	uint8_t i;
	uint8_t ack_status;

	// Send 8 bits (LSB first)
	for (i = 0; i < 8; i++) {
		CLK_LOW(tm1637);
		TM1637_Delay();

		// Output the bit (LSB is bit 0)
		if (data & 0x01) {
			DIO_HIGH(tm1637);
		} else {
			DIO_LOW(tm1637);
		}
		TM1637_Delay();

		CLK_HIGH(tm1637);
		TM1637_Delay();
		data >>= 1; // Shift right for LSB first
	}

	// Read ACK from TM1637 (9th clock cycle)
	CLK_LOW(tm1637);
	TM1637_Delay();

	// Switch DIO to Input mode to read the ACK
	DIO_AsInput(tm1637);
	TM1637_Delay();

	CLK_HIGH(tm1637); // Pulse clock to read ACK
	TM1637_Delay();

	// Check if the TM1637 pulled the line LOW (ACK = GPIO_PIN_RESET)
	ack_status = (DIO_READ(tm1637) == GPIO_PIN_RESET);

	CLK_LOW(tm1637);
	TM1637_Delay();

	// Switch DIO back to Output mode for next transmission
	DIO_AsOutput(tm1637);

	return ack_status;
}

/**
 * @brief Helper function to write a sequence of segments to the display.
 * @param seg_data Pointer to an array of 4 segment bytes (D3, D2, D1, D0).
 * @param start_addr The starting address offset (usually 0).
 */
static void TM1637_WriteDisplayData(TM1637_t* tm1637, uint8_t seg_data[4], uint8_t start_addr) {
	// Send Data Command (Auto Address Increment 0x40)
	TM1637_Start(tm1637);
	TM1637_SendByte(ADDR_AUTO);
	TM1637_Stop(tm1637);

	// Send Address Command (Start at 0xC0) and 4 data bytes
	TM1637_Start(tm1637);
	TM1637_SendByte(tm1637, START_CMD | (start_addr & 0x03)); // Start address C0H, C1H, etc.

	for (int i = 0; i < 4; i++) {
		TM1637_SendByte(tm1637, seg_data[i]);
	}
	TM1637_Stop(tm1637);
}

// ===================================================================================
// PUBLIC API IMPLEMENTATION
// ===================================================================================

void TM1637_Init(TM1637_t* tm1637, GPIO_TypeDef* clk_port,
		uint16_t clk_pin, GPIO_TypeDef* dio_port, uint16_t dio_pin) {

	// Init the tm1637 struct
	tm1637->clk_port = clk_port;
	tm1637->clk_pin = clk_pin;
	tm1637->dio_port = dio_port;
	tm1637->dio_pin = dio_pin;

	uint8_t blank_data[4] = { segment_codes[10], segment_codes[10],
			segment_codes[10], segment_codes[10] };
	TM1637_WriteDisplayData(tm1637, blank_data, 0);
	TM1637_SetBrightness(tm1637, BRIGHTNESS_MAX);
}

void TM1637_SetBrightness(TM1637_t* tm1637, uint8_t brightness) {
	// Brightness level 0-7 is safe
	if (brightness > 7)
		brightness = 7;

	// Display Control Command is 0x88 | (brightness | display_on_flag)
	uint8_t ctrl_byte = 0x88 | brightness;

	TM1637_Start(tm1637);
	TM1637_SendByte(tm1637, ctrl_byte);
	TM1637_Stop(tm1637);
}

void TM1637_DisplayDec(TM1637_t* tm1637, uint16_t val, bool useDot) {
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
	TM1637_WriteDisplayData(tm1637, segments_to_display, 0);
}
