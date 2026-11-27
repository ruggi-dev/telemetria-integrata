#ifndef UPS_POWER_MODE_H
#define UPS_POWER_MODE_H

typedef enum {
    UPS_MODE_NO_POWER = 1,
    UPS_MODE_BYPASS = 2,
    UPS_MODE_MAINS = 3,
    UPS_MODE_BATTERY = 4,
    UPS_MODE_MAINS_ECO = 5,
    UPS_MODE_BATTERY_ECO = 6,
    UPS_MODE_UNKNOWN = 255
} ups_power_mode_t;

#endif
