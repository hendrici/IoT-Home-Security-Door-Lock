/* --------------------------- Include Statements --------------------------- */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <driver/gpio.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "driver/ledc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

/* ---------------------------- Macro Definitions --------------------------- */
#define LOCK_STATUS_TOPIC "brendan/lockStatus/"
#define PIN_OUTPUT_TOPIC "brendan/pinEntry/"

#define PIN_SIZE 4

#define SERVO_PIN GPIO_NUM_4

#define LCD_Enable GPIO_NUM_0
#define LCD_RS GPIO_NUM_19
#define LCD_DB4 GPIO_NUM_32
#define LCD_DB5 GPIO_NUM_33
#define LCD_DB6 GPIO_NUM_16
#define LCD_DB7 GPIO_NUM_17

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO (4) // Define the output GPIO
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT
#define LEDC_DUTY_LOCKED (((1 << 13) - 1) * 0.14)
#define LEDC_DUTY_UNLOCKED (((1 << 13) - 1) * 0.07)
#define LEDC_FREQUENCY (50)

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

int colPins[COLS] = {COL_1_PIN, COL_2_PIN, COL_3_PIN};

#define CONFIG_BROKER_URL "mqtt://test.mosquitto.org/"

/* ---------------------------- Global Variables ---------------------------- */
static const char *TAG = "MQTT_EXAMPLE";

esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = CONFIG_BROKER_URL,
};

esp_mqtt_client_handle_t client;

// Variables for tracking key press state
volatile bool keyWasPressed = false;
int lastKey;


int pin[PIN_SIZE] = {1, 2, 3, 4};

int enteredPin[PIN_SIZE];

/* --------------------------- Function Prototypes -------------------------- */
void ledBlink(void *pvParams);
static void logErrorIfNonzero(const char *message, int error_code);
void printDeviceInfo(void);
void lockBolt(void);
void unlockBolt(void);
void lockInit(void);
void initLCD(void);
void initSequenceLCD(void);
void pulseEnable(void);
void push_nibble(uint8_t var);
void push_byte(uint8_t var);
void commandWrite(uint8_t var);
void dataWrite(uint8_t var);
void writeLockScreen(void);
void writeUnlockScreen(void);
void writeIncorrectPinScreen(void);
void writeEnterPinScreen(void);
void writePinEntry(uint8_t pinLocation, char pinNum);
void printToLCD(uint8_t numStrings, char **strings, uint8_t startLine);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data);
static void mqtt_app_start(void);
void initKeypad(void);
int Keypad_Read(void);
void Keypad_Task(void *arg);

/* ------------------------------------------------------------------------ */

/**
 * @brief Main function for application
 */
void app_main(void) {
    printDeviceInfo();

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes",
        esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();

    initLCD();
    writeEnterPinScreen();

    lockInit();
    
    initKeypad();
    xTaskCreate(Keypad_Task, "Keypad_Task", 5096, NULL, 10, NULL);
}

/**
 * @brief Checks the inputted PIN code, returns if correct or not
 *
 * @param entry PIN code in form of integer array
 * @param size number of characters within PIN code
 * @return boolean value whether or not PIN code is correct
 */
bool checkPin(int *entry, int size)
{
    if (size == PIN_SIZE)
    {
        for (int i = 0; i < PIN_SIZE; i++)
        {
            printf("%d : %d \n", entry[i], pin[i]);
            if (entry[i] != pin[i])
            {
                lockBolt();
                return false;
            }
        }
        unlockBolt();
        return true;
    }
    lockBolt();
    return false;
}

/**
 * @brief A function to log the error that the mqtt callback had if an
 *        unexpected response is returned
 *
 * @param message the message of what error it is
 * @param error_code the error code of the mqtt error
 */
static void logErrorIfNonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/**
 * @brief Prints the device information upon startup
 */
void printDeviceInfo(void)
{
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK)
    {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n",
           esp_get_minimum_free_heap_size());
}

/**
 * @brief Updates the duty cycle of the servo to lock the deadbolt
 */
void lockBolt(void)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_LOCKED);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    esp_mqtt_client_publish(client, LOCK_STATUS_TOPIC, "locked", 0, 1, 0);
}

/**
 * @brief Updates the duty cycle of the servo to unlock the deadbolt
 */
void unlockBolt(void)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_UNLOCKED);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    esp_mqtt_client_publish(client, LOCK_STATUS_TOPIC, "unlocked", 0, 1, 0);
}

/**
 * @brief Initializes a timer for PWM signal to the servo
 */
void lockInit(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_OUTPUT_IO,
        .duty = 0, // Set duty to 0%
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

/**
 * @brief Initializes pins used by the LCD and runs iniatialization sequence
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

    // turns display and cursor ON, blinking
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
    printToLCD(1, message, 1);
    vTaskDelay(2000/portTICK_PERIOD_MS);
}

/**
 * @brief Future method to be implemented, will include data to be written when in the locked state
 */
void writeLockScreen(void) {
    char *message[2] = {"Locked"};    
    printToLCD(1, message, 1);
    vTaskDelay(2000/portTICK_PERIOD_MS);
    writeEnterPinScreen();
}

/**
 * @brief
 */
void writeIncorrectPinScreen(void)  {
    char *message[2] = {"Incorrect PIN"};    
    printToLCD(1, message, 1);
    vTaskDelay(2000/portTICK_PERIOD_MS);
    writeEnterPinScreen();
}

/**
 * @brief
 */
void writeEnterPinScreen(void)  {
    char *message[3] = {"Enter PIN:","****"};
    printToLCD(2, message, 2);
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
void printToLCD(uint8_t numStrings, char **strings, uint8_t startLine) {
    uint8_t count = startLine;
    commandWrite(1);
    vTaskDelay(20 / portTICK_PERIOD_MS);

    for (int i = 0 ; i < numStrings ; i++) {
        switch (count) {
            case 1 :
                commandWrite(0x85);        // first line
                break;
            case 2 :
                commandWrite(0xC3);        // second line
                break;
            case 3 :
                commandWrite(0x96);        // third line
                break;
            case 4 :
                commandWrite(0xD5);        // fourth line
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

/**
 * @brief Takes the mqtt message and converts it into an array of integers
 *        This number is then compared to the stored pin and the deadbolt
 *        is locked/unlocked accordingly
 *
 * @param kLen The length of the string
 * @param input A string array of the input pin written as characters and converted
 *              to integers
 */
void mqtt_pin_to_int_array(uint32_t kLen, char *input)
{
    int enteredPin[kLen];
    uint32_t i = 0;
    for (i = 0; i < kLen; i++)
    {
        enteredPin[i] = input[i] - '0';
    }
    for (i = 0; i < kLen; i++)
    {
        printf("%d ", enteredPin[i]);
    }
    printf("\n");

    checkPin(enteredPin, kLen);
}

/**
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler
 *              (always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event,
 *                      esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG,
             "Event dispatched from event loop base=%s, event_id=%" PRIi32 "",
             base, event_id);

    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d",
                 event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d",
                 event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d",
                 event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        mqtt_pin_to_int_array(event->data_len, event->data);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            logErrorIfNonzero("reported from esp-tls",
                              event->error_handle->esp_tls_last_esp_err);
            logErrorIfNonzero("reported from tls stack",
                              event->error_handle->esp_tls_stack_err);
            logErrorIfNonzero("captured as transport's socket errno",
                              event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)",
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

/**
 * @brief Establishes connection to the mqtt client and publishes locked since the
 *        program starts with the deadbolt locked. Then subscribes to the topic the
 *        mobile app is sending the pin to.
 */
static void mqtt_app_start(void)
{
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    esp_mqtt_client_publish(client, LOCK_STATUS_TOPIC, "locked", 0, 0, 0);
    esp_mqtt_client_subscribe(client, PIN_OUTPUT_TOPIC, 1);
}

int Keypad_Read(void)
{

    uint8_t col, row = 0;
    int num = -1;

    for (col = 0; col < 3; col++)
    {
        // Set all cols to input
        gpio_set_direction(COL_1_PIN, GPIO_MODE_INPUT);
        gpio_set_direction(COL_2_PIN, GPIO_MODE_INPUT);
        gpio_set_direction(COL_3_PIN, GPIO_MODE_INPUT);

        //  set current col to output
        gpio_set_direction(colPins[col], GPIO_MODE_OUTPUT);
        gpio_set_level(colPins[col], 0);


        if (gpio_get_level(ROW_1_PIN) == 0)
        {
            if (!gpio_get_level(ROW_1_PIN))
            {
                while (gpio_get_level(ROW_1_PIN) == 0)
                    ;
                row = 1;
            }
        }
        else if (gpio_get_level(ROW_2_PIN) == 0)
        {
            if (!gpio_get_level(ROW_2_PIN))
            {
                while (gpio_get_level(ROW_2_PIN) == 0)
                    ;
                row = 2;
            }
        }
        else if (gpio_get_level(ROW_3_PIN) == 0)
        {
            if (!gpio_get_level(ROW_3_PIN))
            {
                while (gpio_get_level(ROW_3_PIN) == 0)
                    ;
                row = 3;
            }
        }
        else if (gpio_get_level(ROW_4_PIN) == 0)
        {
            if (!gpio_get_level(ROW_4_PIN))
            {
                while (gpio_get_level(ROW_4_PIN) == 0)
                    ;
                row = 4;
            }
        }

        if (row != 0)
        { // if one of the inputs is low then a key is pressed
            break;
        }

        // End of for iteration, no row selected return 0
        if (col == 2)
        {
            return -1;
        }
    }

    gpio_set_direction(COL_1_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(COL_2_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(COL_3_PIN, GPIO_MODE_INPUT);


    if (row == 1)
    { // key in row 0
        num = col + 1;
    }

    if (row == 2)
    { // key in row 1
        num = 3 + col + 1;
    }

    if (row == 3)
    { // key in row 2
        num = 6 + col + 1;
    }

    if (row == 4)
    { // key in row 3
        num = 9 + col + 1;
    }

    vTaskDelay(200 / portTICK_PERIOD_MS);
    
    if (num == 11)  {
        num = 0;
    }
    lastKey = num;
    return num;
}

void initKeypad()
{
    gpio_set_direction(ROW_1_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(ROW_2_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(ROW_3_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(ROW_4_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en(ROW_1_PIN);
    gpio_pullup_en(ROW_2_PIN);
    gpio_pullup_en(ROW_3_PIN);
    gpio_pullup_en(ROW_4_PIN);

    gpio_set_direction(COL_1_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(COL_2_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(COL_3_PIN, GPIO_MODE_INPUT);

    gpio_set_level(ROW_1_PIN, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(ROW_2_PIN, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(ROW_3_PIN, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(ROW_4_PIN, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(COL_1_PIN, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(COL_2_PIN, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(COL_3_PIN, 1);
}

void Keypad_Task(void *arg)
{

    int val = -1;
    int i = 0;

    while (1)
    {
        val = Keypad_Read();

        if (val != -1)
        {
            if (lastKey == 10)  {
                //do asterisk functionality
                lockBolt();
                writeLockScreen();
                for (int i = 0; i < 4; i++) {
                    enteredPin[i] = -1;
                }
            } else if (lastKey == 12)   {
                //do pound key functionality
                if (checkPin(enteredPin,PIN_SIZE))  {
                    writeUnlockScreen();
                } else  {
                    writeIncorrectPinScreen();
                }
                for (int i = 0; i < 4; i++) {
                    enteredPin[i] = -1;
                }
            } else  {
                enteredPin[i] = lastKey;
                writePinEntry(i,enteredPin[i]+'0');
                i++;
                printf("Number = %d\n", lastKey);
                val = -1;
                if (i == PIN_SIZE)
                {
                    //     printf(" Pin  code: ");
                    //     for(int j = 0; j < PIN_SIZE; j++)
                    //         printf("%d", enteredPin[j]);
                    i = 0;
                    //     printf("\n");
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
