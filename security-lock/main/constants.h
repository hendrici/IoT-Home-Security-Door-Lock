#ifndef _CONSTANTS_H
#define _CONSTANTS_H


#define LOCK_STATUS_TOPIC  "brendan/lockStatus/"
#define PIN_OUTPUT_TOPIC   "brendan/pinEntry/"

#define LED_PIN 2
#define SERVO_PIN 4

#define LCD_Enable              GPIO_NUM_0
#define LCD_RS                  GPIO_NUM_19
#define LCD_DB4                 GPIO_NUM_32
#define LCD_DB5                 GPIO_NUM_33
#define LCD_DB6                 GPIO_NUM_16
#define LCD_DB7                 GPIO_NUM_17

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (4)  // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT
#define LEDC_DUTY_LOCKED        (((1 << 13) - 1) * 0.14)
#define LEDC_DUTY_UNLOCKED      (((1 << 13) - 1) * 0.07)
#define LEDC_FREQUENCY          (50)

#define MAX_STRING_SIZE 40
#define NUMBER_OF_STRING 4


#define ROWS 4
#define COLS 3
#define ROW_1_PIN GPIO_NUM_18
#define ROW_2_PIN GPIO_NUM_27
#define ROW_3_PIN GPIO_NUM_26
#define ROW_4_PIN GPIO_NUM_25
#define COL_1_PIN GPIO_NUM_23
#define COL_2_PIN GPIO_NUM_22
#define COL_3_PIN GPIO_NUM_21

#define PIN_SIZE 4
#define CONFIG_BROKER_URL       "mqtt://test.mosquitto.org/"

#endif
