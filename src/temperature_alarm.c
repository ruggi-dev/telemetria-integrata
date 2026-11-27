#include <errno.h>
#include <stddef.h>

#include "temperature_alarm.h"

static void reset_sample_counts(temperature_alarm_state_t *state)
{
    state->consecutive_high_samples = 0;
    state->consecutive_recovery_samples = 0;
}

int temperature_alarm_update(
    temperature_alarm_state_t *state,
    int sample_valid,
    int temperature_tenths_celsius,
    int threshold_tenths_celsius,
    int hysteresis_tenths_celsius,
    unsigned int confirmation_samples,
    temperature_alarm_event_t *event
)
{
    if (state == NULL || event == NULL ||
        (sample_valid != 0 && sample_valid != 1) ||
        state->active < 0 || state->active > 1 ||
        hysteresis_tenths_celsius < 0 ||
        confirmation_samples == 0) {
        errno = EINVAL;
        return -1;
    }

    *event = TEMPERATURE_ALARM_NO_CHANGE;
    if (!sample_valid) {
        reset_sample_counts(state);
        return 0;
    }

    if (!state->active) {
        state->consecutive_recovery_samples = 0;
        if (temperature_tenths_celsius < threshold_tenths_celsius) {
            state->consecutive_high_samples = 0;
            return 0;
        }

        if (state->consecutive_high_samples < confirmation_samples) {
            state->consecutive_high_samples++;
        }
        if (state->consecutive_high_samples >= confirmation_samples) {
            state->active = 1;
            reset_sample_counts(state);
            *event = TEMPERATURE_ALARM_TRIGGERED;
        }
        return 0;
    }

    state->consecutive_high_samples = 0;
    long long recovery_threshold =
        (long long)threshold_tenths_celsius - hysteresis_tenths_celsius;
    if ((long long)temperature_tenths_celsius > recovery_threshold) {
        state->consecutive_recovery_samples = 0;
        return 0;
    }

    if (state->consecutive_recovery_samples < confirmation_samples) {
        state->consecutive_recovery_samples++;
    }
    if (state->consecutive_recovery_samples >= confirmation_samples) {
        state->active = 0;
        reset_sample_counts(state);
        *event = TEMPERATURE_ALARM_RECOVERED;
    }
    return 0;
}
