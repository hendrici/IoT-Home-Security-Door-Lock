#ifndef _LCD_H
#define _LCD_H

#include <stdint.h>

void initLCD(void);
void initSequenceLCD(void);
void pulseEnable(void);
void push_nibble(uint8_t var);
void push_byte(uint8_t var);
void commandWrite(uint8_t var);
void dataWrite(uint8_t var);
void writeEnterPinScreen(void);
void writePinEntry(uint8_t pinLocation, char pinNum);
void printToLCD(uint8_t numStrings, char **strings, uint8_t startLine);

#endif 
