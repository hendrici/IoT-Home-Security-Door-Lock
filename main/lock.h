#ifndef _LOCK_H
#define _LOCK_H

/**
 * @brief Updates the duty cycle of the servo to lock the deadbolt
 */
void lockBolt(void);

/**
 * @brief Updates the duty cycle of the servo to unlock the deadbolt
 */
void unlockBolt(void);

/**
 * @brief Initializes a timer for PWM signal to the servo
 */
void lockInit(void);

#endif