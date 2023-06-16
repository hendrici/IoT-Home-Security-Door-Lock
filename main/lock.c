#include "lock.h"
#include "constants.h"
#include "ledc_types.h"

/**
 * @brief Updates the duty cycle of the servo to lock the deadbolt
 */
void lockBolt(void) {
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_LOCKED);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

