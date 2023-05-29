/************************** INCLUDE STATEMENTS *******************************/
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
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

#define LOCK_STATUS_TOPIC  "brendan/lockStatus/"
#define PIN_OUTPUT_TOPIC   "brendan/pinEntry/"

#define LED_PIN 2
#define SERVO_PIN 4

#define LCD_Enable              GPIO_NUM_22
#define LCD_RS                  GPIO_NUM_23
#define LCD_DB4                 GPIO_NUM_32
#define LCD_DB5                 GPIO_NUM_33
#define LCD_DB6                 GPIO_NUM_25
#define LCD_DB7                 GPIO_NUM_26

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (4) // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY_LOCKED        (((1 << 13) - 1) * 0.14) //1.5ms ~Center
#define LEDC_DUTY_UNLOCKED      (((1 << 13) - 1) * 0.07) //~45 degrees off center
#define LEDC_FREQUENCY          (50)

#define MAX_STRING_SIZE 40
#define NUMBER_OF_STRING 4

#define CONFIG_BROKER_URL       "mqtt://test.mosquitto.org/"

static const char *TAG = "MQTT_EXAMPLE";

esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = CONFIG_BROKER_URL,
};

esp_mqtt_client_handle_t client;

char arr[NUMBER_OF_STRING][MAX_STRING_SIZE] = {     // 2 dimensional array to store strings

      "Hector", "Garcia", "EGR", "226"
};

int pinSize = 6;
int pin[6] = {1,2,3,4,5,6};

/************************** FUNCTION PROTOTYPES *******************************/
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
void writeEnterPinScreen(void);
void printToLCD(void);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void mqtt_app_start(void);

/************************** FUNCTIONS *******************************/
void app_main(void) {
    printDeviceInfo();

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    /* WIFI FUNCTIONALITY
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();
    
    */

    initLCD();
    writeEnterPinScreen();
    
    // lockInit();
    // xTaskCreate(&led_blink,"LED_BLINK",2048,NULL,5,NULL);
}

bool checkPin(int *entry, int size){
    if(size == pinSize){
        for(int i = 0; i < pinSize; i++){
            printf("%d : %d \n", entry[i], pin[i]);
            if(entry[i] != pin[i]){
                return false;
            }
        }
        return true;
    }
    return false;
}

void ledBlink(void *pvParams) {
    esp_rom_gpio_pad_select_gpio(LED_PIN);
    gpio_set_direction (LED_PIN,GPIO_MODE_OUTPUT);
    while (1) { 
        gpio_set_level(LED_PIN,0);
        // unlockBolt();
        vTaskDelay(4000/portTICK_PERIOD_MS);
        // lockBolt();
        gpio_set_level(LED_PIN,1);
        vTaskDelay(4000/portTICK_PERIOD_MS);    
    }
}

static void logErrorIfNonzero(const char *message, int error_code)   {
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

void printDeviceInfo(void)  {
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
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
}

void lockBolt(void){
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_LOCKED);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    esp_mqtt_client_publish(client, LOCK_STATUS_TOPIC ,"locked",0,1,0);
}

void unlockBolt(void){
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_UNLOCKED);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    esp_mqtt_client_publish(client, LOCK_STATUS_TOPIC ,"unlocked",0,1,0);
}

void lockInit(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 50 Hz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void initLCD(void) {
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

void initSequenceLCD(void) {
    vTaskDelay(1/portTICK_PERIOD_MS);

    // reset sequence
    commandWrite(0x3); 
    vTaskDelay(10/portTICK_PERIOD_MS);
    commandWrite(0x3); 
    vTaskDelay(1/portTICK_PERIOD_MS);
    commandWrite(0x3); 
    vTaskDelay(10/portTICK_PERIOD_MS);

    // set to 4-bit mode
    commandWrite(0x2);
    vTaskDelay(1/portTICK_PERIOD_MS);
    commandWrite(0x2);
    vTaskDelay(1/portTICK_PERIOD_MS);

    // // 2 lines, 5x7 format
    // commandWrite(0x8);
    // vTaskDelay(1/portTICK_PERIOD_MS);

    // turns display and cursor ON, blinking
    commandWrite(0xF);
    vTaskDelay(1/portTICK_PERIOD_MS);

    // clear display, move cursor to home
    commandWrite(0x1);
    vTaskDelay(1/portTICK_PERIOD_MS);

    // increment cursor
    commandWrite(0x6);
    vTaskDelay(1/portTICK_PERIOD_MS);
}

void pulseEnable(void) {

    gpio_set_level(LCD_Enable, 0); 
    vTaskDelay(1/portTICK_PERIOD_MS);

    gpio_set_level(LCD_Enable, 1);
    vTaskDelay(1/portTICK_PERIOD_MS);

    gpio_set_level(LCD_Enable, 0); 
}

void push_nibble(uint8_t var) { 

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

void push_byte(uint8_t var) {
    push_nibble(var >> 4);
    push_nibble(var & 0x0F);
    vTaskDelay(1/portTICK_PERIOD_MS);
}

void commandWrite(uint8_t var) {
    gpio_set_level(LCD_RS, 0); 
    vTaskDelay(1/portTICK_PERIOD_MS);
    push_byte(var); 
}

void dataWrite(uint8_t var) {
    gpio_set_level(LCD_RS, 1); 
    vTaskDelay(1/portTICK_PERIOD_MS);
    push_byte(var); 
}

void changeScreenStateLCD(void) {

}

void writeUnlockScreen(bool isRemote)    {

}

void writeLockScreen(bool isRemote) {
    if (isRemote)   {

    } else  {

    }
}

void printToLCD(void) {

    int count = 0;

        int i = 0;
        int j = 0;
        
        commandWrite(1);
        vTaskDelay(20/portTICK_PERIOD_MS);
 
        commandWrite(0x85);        // Center of first line
        vTaskDelay(20/portTICK_PERIOD_MS);

        for ( i = 0 ; i < NUMBER_OF_STRING ; i++) {


            for (j = 0 ; j < (strlen(arr[i])) ; j++) { // changed i to j

                dataWrite(arr[i][j]);
                vTaskDelay(20/portTICK_PERIOD_MS);

            }

            count++;     // increment count so the next text can be written on the next line


            switch (count) {

            case 1 :
                commandWrite(0xC5);        // Center of second line
        vTaskDelay(20/portTICK_PERIOD_MS);

                break;

            case 2 :
                commandWrite(0x96);        // Center of third line
        vTaskDelay(20/portTICK_PERIOD_MS);

                break;
            case 3 :
                commandWrite(0xD6);        // Center of third line
        vTaskDelay(20/portTICK_PERIOD_MS);

                break;
            }

        }
}

void writeEnterPinScreen(void)  {
    uint8_t i;
    char name[33] = "    Isaiah H.         EGR       ";         // message for line 1 & 3
    char name2[33] = "   Triston G.         227       ";        // message for line 2 & 4

    for(i = 0; i < 32; i++){        // prints message for line 1 & 3
        dataWrite(name[i]);
        vTaskDelay(10/portTICK_PERIOD_MS);
    }

    commandWrite(0xC0);             // move to line 2 (address 0x40)-> 0b1100 0000

    for(i = 0; i < 32; i++){        // prints message for line 2 & 4
        dataWrite(name2[i]);
        vTaskDelay(10/portTICK_PERIOD_MS);
    }

}

void mqtt_pin_to_int_array(uint32_t len, char *input){

    int enteredPin[len];
    uint32_t i=0;
    for(i=0; i<len; i++){
        enteredPin[i] = input[i] - '0';
    }
    for(i=0; i < len; i++){
        printf("%d ",enteredPin[i]);
    }
    printf("\n");

    if(checkPin(enteredPin, len)){
        unlockBolt();
    }
    else{
        lockBolt();
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)   {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        mqtt_pin_to_int_array(event->data_len, event->data);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            logErrorIfNonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            logErrorIfNonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            logErrorIfNonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    esp_mqtt_client_publish(client, LOCK_STATUS_TOPIC ,"locked",0,0,0);
    esp_mqtt_client_subscribe(client, PIN_OUTPUT_TOPIC, 1);
}