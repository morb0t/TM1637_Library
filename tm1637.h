/*
 * tm1637.h
 *
 *  Created on: Nov 15, 2025
 *      Author: Anouar ELHARDA
 */

#ifndef __TM1637_H_
#define __TM1637_H_

#include <stdbool.h>

// --- Command Definitions ---
#define ADDR_AUTO	0x40
#define ADDR_FIXED	0x44
#define START_CMD	0xC0

// --- Display Control Commands (0x80 - 0x8F) ---
// Note: Brightness is controlled by the lower 3 bits (0-7)
#define BRIGHTNESS_MAX	0x8F
#define BRIGHTNESS_MIN	0x88
#define DISPLAY_OFF		0x80

// --- Public Function Prototypes ---
/**
 * @brief Initializes the TM1637 module, clears the display, and sets the max brightness
 */
void TM1637_Init(void);

/**
 * @brief Displays a 4-digit decimal number on the segment display.
 * @param val the 16-bits number to display (max 9999).
 * @param useDot if true, the middle dot is illuminated.
 */
void TM1637_DisplayDec(uint16_t val, bool useDot);
/**
 * @brief Sets the display brightness level.
 * @param brightness Brightness level (0=DISPLAY_OFF to 7=Max).
 */
void TM1637_SetBrightness(uint8_t brightness);

// --- External Segment Code Lookup Table ---
// Digit 0-9 codes, plus 0x00 for BLANK
extern const uint8_t segment_codes[];
#endif /* INC_TM1637_H_ */
