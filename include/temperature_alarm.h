#ifndef TEMPERATURE_ALARM_H
#define TEMPERATURE_ALARM_H

typedef enum {
    TEMPERATURE_ALARM_NO_CHANGE,
    TEMPERATURE_ALARM_TRIGGERED,
    TEMPERATURE_ALARM_RECOVERED
} temperature_alarm_event_t;

typedef struct {
    unsigned int consecutive_high_samples;
    unsigned int consecutive_recovery_samples;
    int active;
} temperature_alarm_state_t;

int temperature_alarm_update(
    temperature_alarm_state_t *state,
    int sample_valid,
    int temperature_tenths_celsius,
    int threshold_tenths_celsius,
    int hysteresis_tenths_celsius,
    unsigned int confirmation_samples,
    temperature_alarm_event_t *event
);

#endif
