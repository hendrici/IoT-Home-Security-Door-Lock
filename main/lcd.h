#ifndef _LCD_H
#define _LCD_H

#include <stdint.h>

/**
 * @brief Initializes pins used by the LCD and runs initialization sequence
 */
void initLCD(void);

/**
 * @brief Writes "Enter Pin" to the screen
 */
void writeEnterPinScreen(void);

/**
 * @brief Writes "Locked" to the screen
 */
void writeLockScreen(void);

/**
 * @brief Writes "Unlocked" to the screen
 */
void writeUnlockScreen(void);

/**
 * @brief Writes "Incorrect PIN" to the screen
 */
void writeIncorrectPinScreen(void);

/**
 * @brief Writes a pin character to the screen
 */
void writePinEntry(uint8_t pinLocation, char pinNum);

#endif 
