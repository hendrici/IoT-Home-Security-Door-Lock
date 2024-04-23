#include <string.h>
#include <stdint.h>
#include <driver/gpio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "lcd.h"
#include "constants.h"

/* --------------------------- Function Prototypes -------------------------- */
void initSequenceLCD(void);
void pulseEnable(void);
void push_nibble(uint8_t var);
void push_byte(uint8_t var);
void commandWrite(uint8_t var);
void dataWrite(uint8_t var);
void printToLCD(uint8_t numStrings, char **strings, uint8_t *xOffset, uint8_t yOffset);


/* ---------------------------- Global Functions ---------------------------- */

/**
 * @brief Initializes pins used by the LCD and runs initialization sequence
 */
void initLCD(void)
{
    // Initialize LCD GPIOs as outputs
    gpio_set_direction(LCD_DB4, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_DB5, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_DB6, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_DB7, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_RS, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_Enable, GPIO_MODE_OUTPUT);

    // Drive LCD pins LOW
    gpio_set_level(LCD_DB4, 0);
    gpio_set_level(LCD_DB5, 0);
    gpio_set_level(LCD_DB6, 0);
    gpio_set_level(LCD_DB7, 0);
    gpio_set_level(LCD_RS, 0);
    gpio_set_level(LCD_Enable, 0);

    initSequenceLCD();
}

/**
 * @brief Initialization command sequence for HD44780 LCD controller
 */
void initSequenceLCD(void)
{
    vTaskDelay(1 / portTICK_PERIOD_MS);

    // reset sequence
    commandWrite(0x3);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    commandWrite(0x3);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    commandWrite(0x3);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    // set to 4-bit mode
    commandWrite(0x2);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    commandWrite(0x2);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    // 2 lines, 5x7 format
    // commandWrite(0x8);
    // vTaskDelay(1/portTICK_PERIOD_MS);

    // turns display and cursor OFF
    commandWrite(0xC);
    vTaskDelay(1/portTICK_PERIOD_MS);

    // clear display, move cursor to home
    commandWrite(0x1);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    // increment cursor
    commandWrite(0x6);
    vTaskDelay(1 / portTICK_PERIOD_MS);
}

/**
 * @brief Pulses the enable pin on the LCD
 */
void pulseEnable(void)
{
    gpio_set_level(LCD_Enable, 0);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    gpio_set_level(LCD_Enable, 1);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    gpio_set_level(LCD_Enable, 0);
}

/**
 * @brief Pushes a nibble (4 bits) to the data pins on the LCD
 *
 * @param var 4 bit number to be sent to the LCD
 */
void push_nibble(uint8_t var)
{
    // Drive Pins Low (clear pins)
    gpio_set_level(LCD_DB7, 0);
    gpio_set_level(LCD_DB6, 0);
    gpio_set_level(LCD_DB5, 0);
    gpio_set_level(LCD_DB4, 0);

    // Set Respective Pin
    gpio_set_level(LCD_DB7, var >> 3);
    gpio_set_level(LCD_DB6, (var >> 2) & 1);
    gpio_set_level(LCD_DB5, (var >> 1) & 1);
    gpio_set_level(LCD_DB4, var & 1);

    pulseEnable();
}

/**
 * @brief Pushes a byte (8 bits) to the data pins on the LCD, 1 nibble at a time
 *
 * @param var 8 bit number to be sent to the LCD
 */
void push_byte(uint8_t var)
{
    push_nibble(var >> 4);
    push_nibble(var & 0x0F);
    vTaskDelay(1 / portTICK_PERIOD_MS);
}

/**
 * @brief Writes a commmand to the LCD
 *
 * @param var command to be written
 */
void commandWrite(uint8_t var)
{
    gpio_set_level(LCD_RS, 0);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    push_byte(var);
}

/**
 * @brief Writes data to the LCD
 *
 * @param var data to be written (characters)
 */
void dataWrite(uint8_t var)
{
    gpio_set_level(LCD_RS, 1);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    push_byte(var);
}

/**
 * @brief Future method to be implemented, will include data to be written when in the unlocked state
 */
void writeUnlockScreen(void)    {
    char *message[2] = {"Unlocked"};
    uint8_t xOffset[5] = {0,4,0,0};
    printToLCD(1, message, xOffset, 1);
    vTaskDelay(2000/portTICK_PERIOD_MS);
}

/**
 * @brief Future method to be implemented, will include data to be written when in the locked state
 */
void writeLockScreen(void) {
    char *message[2] = {"Locked"};
    uint8_t xOffset[5] = {0,5,0,0};    
    printToLCD(1, message, xOffset, 1);
    vTaskDelay(2000/portTICK_PERIOD_MS);
    writeEnterPinScreen();
}

/**
 * @brief
 */
void writeIncorrectPinScreen(void)  {
    char *message[2] = {"Incorrect PIN"};    
    uint8_t xOffset[5] = {0,1,0,0};    
    printToLCD(1, message, xOffset, 1);
    vTaskDelay(2000/portTICK_PERIOD_MS);
    writeEnterPinScreen();
}

/**
 * @brief
 */
void writeEnterPinScreen(void)  {
    char *message[3] = {"Enter PIN:","****"};
    uint8_t xOffset[5] = {0,3,6,0};    
    printToLCD(2, message, xOffset, 1);
}


/**
 * @brief 
 * 
 * @param
 * @param
 */
void writePinEntry(uint8_t pinLocation, char pinNum) {
    if (pinLocation == 0)   {
        writeEnterPinScreen();
        commandWrite(0x96);
        vTaskDelay(20/portTICK_PERIOD_MS);
    }

    dataWrite(pinNum);
    vTaskDelay(20/portTICK_PERIOD_MS);
}

/**
 * @brief Prints strings to four lines of the LCD
 * 
 * @param
 * @param
 * @param
 */
void printToLCD(uint8_t numStrings, char **strings, uint8_t *xOffset, uint8_t yOffset) {
    uint8_t count = yOffset;
    commandWrite(1);
    vTaskDelay(20 / portTICK_PERIOD_MS);

    for (int i = 0 ; i < numStrings ; i++) {
        switch (count) {
            case 0 :
                commandWrite(0x80 + xOffset[0]);        // first line
                break;
            case 1 :
                commandWrite(0xC0 + xOffset[1]);        // second line
                break;
            case 2 :
                commandWrite(0x90 + xOffset[2]);        // third line
                break;
            case 3 :
                commandWrite(0xD0 + xOffset[3]);        // fourth line
                break;
        }

        vTaskDelay(20 / portTICK_PERIOD_MS);

        // increment count so the next text can be written on the next line
        count++;

        for (int j = 0 ; j < (strlen(strings[i])) ; j++) { 
            dataWrite(strings[i][j]);
            vTaskDelay(20/portTICK_PERIOD_MS);
        }
    }
}

