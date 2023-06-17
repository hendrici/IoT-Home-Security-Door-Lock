#ifndef _LCD_H
#define _LCD_H

#include <stdint.h>

/**
 * @brief Initializes pins used by the LCD and runs initialization sequence
 */
void initLCD(void);

/**
 * @brief Writes enter pin to the screen
 */
void writeEnterPinScreen(void);

/**
 * @brief Writes the pin to the screen
 */
void writePinEntry(uint8_t pinLocation, char pinNum);

#endif 
