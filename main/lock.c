#include "lock.h"
#include "constants.h"
#include "driver/ledc.h"
#include "mqtt_client.h"


extern esp_mqtt_client_handle_t client;

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
