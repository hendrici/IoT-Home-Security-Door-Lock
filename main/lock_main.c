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


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "constants.h"
#include "lock.h"
#include "lcd.h"

/* ---------------------------- Global Variables ---------------------------- */
static const char *TAG = "MQTT_EXAMPLE";

esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = CONFIG_BROKER_URL,
};

esp_mqtt_client_handle_t client;

// Variables for tracking key press state
volatile bool keyWasPressed = false;
int lastKey;

int colPins[COLS] = {COL_1_PIN, COL_2_PIN, COL_3_PIN};

int pin[PIN_SIZE] = {1, 2, 3, 4};

int enteredPin[PIN_SIZE];

/* --------------------------- Function Prototypes -------------------------- */
void ledBlink(void *pvParams);
static void logErrorIfNonzero(const char *message, int error_code);
void printDeviceInfo(void);
bool checkPin(int *entry, int size);
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
            } else if (lastKey == 12)   {
                //do pound key functionality
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
                    checkPin(enteredPin,PIN_SIZE);
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
