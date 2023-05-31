#include <stdio.h>
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

// Define the keypad pins
#define KEYPAD_DEBOUNCING 100   ///< time in ms
#define KEYPAD_STACKSIZE  5
#define LED_PIN 2
#define ROWS 4
#define COLS 3
#define ROW_1_PIN GPIO_NUM_25
#define ROW_2_PIN 26
#define ROW_3_PIN GPIO_NUM_27
#define ROW_4_PIN GPIO_NUM_18
#define COL_1_PIN GPIO_NUM_21
#define COL_2_PIN GPIO_NUM_22
#define COL_3_PIN GPIO_NUM_23


// Define the keypad matrix
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

// Variables for tracking key press state
volatile bool keyWasPressed = false;
volatile char lastKey;



#define CONFIG_BROKER_URL "mqtt://test.mosquitto.org/"

static const char *TAG = "MQTT_EXAMPLE";

esp_mqtt_client_handle_t mqtt_client;

void led_blink(void *pvParams) {
    esp_rom_gpio_pad_select_gpio(LED_PIN);
    gpio_set_direction (LED_PIN,GPIO_MODE_OUTPUT);
    while (1) { 
        gpio_set_level(LED_PIN,0);
        vTaskDelay(1000/portTICK_PERIOD_MS);
        gpio_set_level(LED_PIN,1);
        vTaskDelay(1000/portTICK_PERIOD_MS);    
    }
}


static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
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
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
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
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
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
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    esp_mqtt_client_publish(client,"brendan/testing/open","test",0,0,0);
}

void printDeviceInfo(void){
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

uint8_t Keypad_Read(void){

    uint8_t col, row = 0;
    int num = 0;

    for(col = 0; col<3; col++){
        // Set all cols to input
        gpio_set_direction(COL_1_PIN, GPIO_MODE_INPUT);
        gpio_set_direction(COL_2_PIN, GPIO_MODE_INPUT);
        gpio_set_direction(COL_3_PIN, GPIO_MODE_INPUT);

        vTaskDelay(500/portTICK_PERIOD_MS);
        // set current col to output
        gpio_set_direction(COL_1_PIN + col, GPIO_MODE_OUTPUT);
        gpio_set_level(COL_1_PIN + col, 0);  


        // vTaskDelay(pdMS_TO_TICKS(10));
        vTaskDelay(500/portTICK_PERIOD_MS);


        if(gpio_get_level(ROW_1_PIN) == 0){
            //while(gpio_get_level(ROW_1_PIN) == 0);
            row = 1;
        }
        else if(gpio_get_level(ROW_2_PIN) == 0){
            //while(gpio_get_level(ROW_2_PIN) == 0);
            row = 2;
        }
        else if(gpio_get_level(ROW_3_PIN) == 0){
            //while(gpio_get_level(ROW_3_PIN) == 0);
            row = 3;
        }
        else if(gpio_get_level(ROW_4_PIN) == 0){
            //while(gpio_get_level(ROW_4_PIN) == 0);
            row = 4;
        }

        if(row != 0){//if one of the inputs is low then a key is pressed
            break;
        }
    }

    gpio_set_direction(COL_1_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(COL_2_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(COL_3_PIN, GPIO_MODE_INPUT);

    if(col == 3){
        return 0;
    }

    if(row == 1){//key in row 0
        num = col + 1;
    }

    if(row == 2){//key in row 1
       num = 3 + col + 1;
    }

    if(row == 3){//key in row 2
        num = 6 + col + 1;
    }

    if(row == 4){//key in row 3
        num = 9 + col + 1;
    }

    printf("Num: %d\n", num);
    // if (num == 11) num = 0;
    // else if( num == 10) return 10;
    // else if( num == 12) return 0;
    return 1;
}


void app_main(void) {
    printf("Hello World\n");

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
    vTaskDelay(50/portTICK_PERIOD_MS);
    gpio_set_level(ROW_2_PIN, 1);
    vTaskDelay(50/portTICK_PERIOD_MS);
    gpio_set_level(ROW_3_PIN, 1);
    vTaskDelay(50/portTICK_PERIOD_MS);
    gpio_set_level(ROW_4_PIN, 1);
    vTaskDelay(50/portTICK_PERIOD_MS);
    gpio_set_level(COL_1_PIN, 1);
    vTaskDelay(50/portTICK_PERIOD_MS);
    gpio_set_level(COL_2_PIN, 1);
    vTaskDelay(50/portTICK_PERIOD_MS);
    gpio_set_level(COL_3_PIN, 1);


  printf("1\n");

  while (1) {
    Keypad_Read();

    // if (keyWasPressed) {
    //   // Print the last key pressed
    //   printf("Key Pressed: %c\n", lastKey);

    //   // Reset the flag
    //   keyWasPressed = false;
    // }

    vTaskDelay(pdMS_TO_TICKS(1000));  // Adjust the delay as per your requirements
  }
}


// void app_main(void) 
// {
//     printDeviceInfo();

//     ESP_LOGI(TAG, "[APP] Startup..");
//     ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
//     ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

//     esp_log_level_set("*", ESP_LOG_INFO);
//     esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
//     esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
//     esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
//     esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
//     esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
//     esp_log_level_set("outbox", ESP_LOG_VERBOSE);

//     ESP_ERROR_CHECK(nvs_flash_init());
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());

//     /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
//      * Read "Establishing Wi-Fi or Ethernet Connection" section in
//      * examples/protocols/README.md for more information about this function.
//      */
//     ESP_ERROR_CHECK(example_connect());

//     // mqtt_app_start();
//     xTaskCreate(&led_blink,"LED_BLINK",1024,NULL,5,NULL);
// }

