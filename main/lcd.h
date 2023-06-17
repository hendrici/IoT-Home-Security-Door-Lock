#ifndef _LCD_H
#define _LCD_H

#include <stdint.h>

/**
 * @brief Initializes pins used by the LCD and runs initialization sequence
 */
void initLCD(void);
/**
 * @brief
 */
void writeEnterPinScreen(void);
/**
 * @brief
 */
void writePinEntry(uint8_t pinLocation, char pinNum);

#endif 
