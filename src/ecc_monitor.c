#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Net-SNMP's configuration header must precede its API header. */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include "snmp_oid.h"
#include "ups_power_mode.h"

/* Defaults used when command-line options are omitted. */
#define DEFAULT_HOST "172.29.2.30"
#define DEFAULT_DURATION_SECONDS 20
#define DEFAULT_INTERVAL_SECONDS 1
#define DEFAULT_TIMEOUT_MICROSECONDS (1000000L)
#define DEFAULT_CSV_DIRECTORY "output"
#define MAX_DURATION_SECONDS 3600UL
#define MAX_PATH_LENGTH 4096
#define VALUE_BUFFER_SIZE 512
#define UPS1_POWER_MODE_OID "HUAWEI-ECC-MIB::hwUPS2000GAPowerMode.4118"
#define UPS2_POWER_MODE_OID "HUAWEI-ECC-MIB::hwUPS2000GAPowerMode.4119"

typedef enum ups_index {
    UPS_1_INDEX,
    UPS_2_INDEX,
    UPS_COUNT,
    UPS_INDEX_INVALID = -1
} ups_index_t;

typedef enum {
    SENSOR_NAME_SIMPLE,
    SENSOR_NAME_MIB,
    SENSOR_NAME_BOTH
} sensor_name_mode_t;

typedef struct {
    const char *host;
    const char *mib_dir;
    const char *csv_path;
    unsigned int duration;
    unsigned int interval;
    sensor_name_mode_t name_mode;
} monitor_options_t;

typedef enum {
    OPTIONS_ERROR = -1,
    OPTIONS_READY,
    OPTIONS_HELP
} options_result_t;

/* Describes a sensor name, OID, scaling divisor, and unit. */
typedef struct {
    const char *simple_name;
    const char *oid_text;
    unsigned long oid_value[MAX_OID_LEN];
    size_t oid_length;
    /* Values 10 and 1 indicate tenths and no conversion, respectively. */
    unsigned int scale_divisor;
    const char *unit;
} sensor_t;

/* This order also determines the terminal and CSV column order. */
static sensor_t sensors[] = {
    /* Air-conditioner temperatures and power. */
    {
        .simple_name = "Supply air temperature - Air conditioner 1",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010AverageSupplyAirTemperature.4121",
        .scale_divisor = 10,
        .unit = "°C"
    },
    {
        .simple_name = "Supply air temperature - Air conditioner 2",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010AverageSupplyAirTemperature.4122",
        .scale_divisor = 10,
        .unit = "°C"
    },
    {
        .simple_name = "Return air temperature - Air conditioner 1",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010AverageReturnAirTemperature.4121",
        .scale_divisor = 10,
        .unit = "°C"
    },
    {
        .simple_name = "Return air temperature - Air conditioner 2",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010AverageReturnAirTemperature.4122",
        .scale_divisor = 10,
        .unit = "°C"
    },
    {
        .simple_name = "Cold aisle temperature - Air conditioner 1",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010AverageColdAisleTemperature.4121",
        .scale_divisor = 10,
        .unit = "°C"
    },
    {
        .simple_name = "Cold aisle temperature - Air conditioner 2",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010AverageColdAisleTemperature.4122",
        .scale_divisor = 10,
        .unit = "°C"
    },
    {
        .simple_name = "PSU input power - Air conditioner 1",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010PsuInputPower.4121",
        .scale_divisor = 1,
        .unit = "W"
    },
    {
        .simple_name = "PSU input power - Air conditioner 2",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010PsuInputPower.4122",
        .scale_divisor = 1,
        .unit = "W"
    },
    /* Temperature and humidity sensors. */
    {
        .simple_name = "Hot aisle temperature - T/H sensor 1",
        .oid_text = "HUAWEI-ECC-MIB::hwTempHumTempSensor.4123",
        .scale_divisor = 10,
        .unit = "°C"
    },
    {
        .simple_name = "Cold aisle temperature - T/H sensor 2",
        .oid_text = "HUAWEI-ECC-MIB::hwTempHumTempSensor.4124",
        .scale_divisor = 10,
        .unit = "°C"
    },
    {
        .simple_name = "Hot aisle relative humidity - T/H sensor 1",
        .oid_text = "HUAWEI-ECC-MIB::hwTempHumHumSensor.4123",
        .scale_divisor = 10,
        .unit = "%RH"
    },
    {
        .simple_name = "Cold aisle relative humidity - T/H sensor 2",
        .oid_text = "HUAWEI-ECC-MIB::hwTempHumHumSensor.4124",
        .scale_divisor = 10,
        .unit = "%RH"
    },
    /* UPS output power and load. */
    {
        .simple_name = "Server output power - UPS 1",
        .oid_text = "HUAWEI-ECC-MIB::hwUPS2000GAOutputActivePower.4118",
        .scale_divisor = 10,
        .unit = "kW"
    },
    {
        .simple_name = "Server output power - UPS 2",
        .oid_text = "HUAWEI-ECC-MIB::hwUPS2000GAOutputActivePower.4119",
        .scale_divisor = 10,
        .unit = "kW"
    },
    {
        .simple_name = "UPS load - UPS 1",
        .oid_text = "HUAWEI-ECC-MIB::hwUPS2000GALoadRatio.4118",
        .scale_divisor = 10,
        .unit = "%"
    },
    {
        .simple_name = "UPS load - UPS 2",
        .oid_text = "HUAWEI-ECC-MIB::hwUPS2000GALoadRatio.4119",
        .scale_divisor = 10,
        .unit = "%"
    },
    /* Rectifier DC measurements. */
    {
        .simple_name = "DC output power - Rectifier 1",
        .oid_text = "HUAWEI-ECC-MIB::hwPowerModuleDcOutputPower.111",
        .scale_divisor = 10,
        .unit = "W"
    },
    {
        .simple_name = "DC output voltage - Rectifier 1",
        .oid_text = "HUAWEI-ECC-MIB::hwPowerModuleDcOutputVoltServeyValue.111",
        .scale_divisor = 10,
        .unit = "V"
    },
    {
        .simple_name = "DC output current - Rectifier 1",
        .oid_text = "HUAWEI-ECC-MIB::hwPowerModuleShowOutputCurrent.111",
        .scale_divisor = 10,
        .unit = "A"
    },
    {
        .simple_name = "Internal temperature - Rectifier 1",
        .oid_text = "HUAWEI-ECC-MIB::hwPowerModuleInnerTemp.111",
        .scale_divisor = 10,
        .unit = "°C"
    },
    /* Equipment operating states. */
    {
        .simple_name = "Air conditioner 1 status",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010OnorOffStatusOfAirConditioner.4121",
        .scale_divisor = 1,
        .unit = NULL
    },
    {
        .simple_name = "Air conditioner 2 status",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010OnorOffStatusOfAirConditioner.4122",
        .scale_divisor = 1,
        .unit = NULL
    },
    {
        .simple_name = "UPS 1 power mode",
        .oid_text = UPS1_POWER_MODE_OID,
        .scale_divisor = 1,
        .unit = NULL
    },
    {
        .simple_name = "UPS 2 power mode",
        .oid_text = UPS2_POWER_MODE_OID,
        .scale_divisor = 1,
        .unit = NULL
    }
};

#define SENSOR_COUNT (sizeof(sensors) / sizeof(sensors[0]))

/* Print command-line syntax and default values. */
static void print_usage(const char *program)
{
    printf(
        "Usage: %s [-H host] [-M mib_directory] [-o csv_file] "
        "[-d duration_seconds] [-i interval_seconds] "
        "[-n simple|mib|both]\n"
        "Set the SNMPv2c community in the ECC_COMMUNITY environment variable.\n"
        "-n selects the sensor names shown in the terminal (default: simple).\n"
        "Defaults: host %s, duration %d s, interval %d s, "
        "CSV %s/ecc-monitor_YYYY-MM-DD_HH-MM-SS.csv.\n",
        program,
        DEFAULT_HOST,
        DEFAULT_DURATION_SECONDS,
        DEFAULT_INTERVAL_SECONDS,
        DEFAULT_CSV_DIRECTORY
    );
}

/* Convert the -n option into a sensor-name display mode. */
static int parse_sensor_name_mode(
    const char *text,
    sensor_name_mode_t *mode
)
{
    if (strcmp(text, "simple") == 0) {
        *mode = SENSOR_NAME_SIMPLE;
    } else if (strcmp(text, "mib") == 0) {
        *mode = SENSOR_NAME_MIB;
    } else if (strcmp(text, "both") == 0) {
        *mode = SENSOR_NAME_BOTH;
    } else {
        return -1;
    }
    return 0;
}

/* Format a sensor identifier using the selected display mode. */
static int format_sensor_identifier(
    char *buffer,
    size_t buffer_size,
    const sensor_t *sensor,
    sensor_name_mode_t mode
)
{
    int written;
    switch (mode) {
    case SENSOR_NAME_SIMPLE:
        written = snprintf(buffer, buffer_size, "%s", sensor->simple_name);
        break;
    case SENSOR_NAME_MIB:
        written = snprintf(buffer, buffer_size, "%s", sensor->oid_text);
        break;
    default:
        written = snprintf(
            buffer,
            buffer_size,
            "%s | %s",
            sensor->simple_name,
            sensor->oid_text
        );
        break;
    }

    if (written < 0 || (size_t)written >= buffer_size) {
        errno = ENOSPC;
        return -1;
    }
    return 0;
}

/* Validate a duration in seconds and convert it to unsigned int. */
static int parse_positive_seconds(const char *text, unsigned int *value)
{
    char *end = NULL;
    errno = 0;
    /* strtoul reports invalid input and overflow through errno and end. */
    unsigned long parsed = strtoul(text, &end, 10);

    /* Reject empty input, trailing characters, zero, and durations over one hour. */
    if (errno != 0 || end == text || *end != '\0' ||
        parsed == 0 || parsed > MAX_DURATION_SECONDS) {
        return -1;
    }

    *value = (unsigned int)parsed;
    return 0;
}

static options_result_t parse_options(
    int argc,
    char **argv,
    monitor_options_t *options
)
{
    int option;

    while ((option = getopt(argc, argv, "H:M:o:d:i:n:h")) != -1) {
        switch (option) {
        case 'H':
            options->host = optarg;
            break;
        case 'M':
            options->mib_dir = optarg;
            break;
        case 'o':
            options->csv_path = optarg;
            break;
        case 'd':
            if (parse_positive_seconds(optarg, &options->duration) != 0) {
                fprintf(stderr, "Invalid duration: %s\n", optarg);
                return OPTIONS_ERROR;
            }
            break;
        case 'i':
            if (parse_positive_seconds(optarg, &options->interval) != 0) {
                fprintf(stderr, "Invalid interval: %s\n", optarg);
                return OPTIONS_ERROR;
            }
            break;
        case 'n':
            if (parse_sensor_name_mode(optarg, &options->name_mode) != 0) {
                fprintf(
                    stderr,
                    "Invalid sensor-name mode: %s "
                    "(use simple, mib, or both).\n",
                    optarg
                );
                return OPTIONS_ERROR;
            }
            break;
        case 'h':
            print_usage(argv[0]);
            return OPTIONS_HELP;
        default:
            print_usage(argv[0]);
            return OPTIONS_ERROR;
        }
    }

    return OPTIONS_READY;
}

/* Format the current local time to millisecond precision. */
static int format_timestamp(char *buffer, size_t buffer_size)
{
    struct timespec now;
    struct tm local_time;
    char date[32];

    /* Use wall-clock time for the human-readable sample timestamp. */
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return -1;
    }
    if (localtime_r(&now.tv_sec, &local_time) == NULL ||
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &local_time) == 0) {
        errno = EINVAL;
        return -1;
    }

    int written = snprintf(
        buffer,
        buffer_size,
        "%s.%03ld",
        date,
        now.tv_nsec / 1000000L
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        errno = ENOSPC;
        return -1;
    }
    return 0;
}

/* Generate the default CSV path using the local date and time. */
static int make_default_csv_path(char *buffer, size_t buffer_size)
{
    struct timespec now;
    struct tm local_time;
    char date[32];

    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return -1;
    }
    if (localtime_r(&now.tv_sec, &local_time) == NULL ||
        strftime(date, sizeof(date), "%Y-%m-%d_%H-%M-%S", &local_time) == 0) {
        errno = EINVAL;
        return -1;
    }

    int written = snprintf(
        buffer,
        buffer_size,
        "%s/ecc-monitor_%s.csv",
        DEFAULT_CSV_DIRECTORY,
        date
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        errno = ENOSPC;
        return -1;
    }
    return 0;
}

/* Write a quoted CSV field, doubling any quotes inside it. */
static int csv_write_field(FILE *csv, const char *field)
{
    if (fputc('"', csv) == EOF) {
        return -1;
    }

    /* CSV represents an embedded quote as two consecutive quotes. */
    for (const char *character = field; *character != '\0'; character++) {
        if (*character == '"' && fputc('"', csv) == EOF) {
            return -1;
        }
        if (fputc(*character, csv) == EOF) {
            return -1;
        }
    }

    return fputc('"', csv) == EOF ? -1 : 0;
}

/* Append the MIB scaling conversion and unit to a raw SNMP value. */
static int append_sensor_unit(
    char *value,
    size_t value_size,
    const sensor_t *sensor,
    const netsnmp_variable_list *variable
)
{
    if (sensor->unit == NULL || variable == NULL ||
        variable->val.integer == NULL ||
        (variable->type != ASN_INTEGER && variable->type != ASN_UNSIGNED)) {
        return 0;
    }

    char formatted[VALUE_BUFFER_SIZE];
    long raw_value = *variable->val.integer;
    int written;

    if (sensor->scale_divisor == 10) {
        written = snprintf(
            formatted,
            sizeof(formatted),
            "%s (%.1f %s)",
            value,
            (double)raw_value / 10.0,
            sensor->unit
        );
    } else {
        written = snprintf(
            formatted,
            sizeof(formatted),
            "%s (%ld %s)",
            value,
            raw_value,
            sensor->unit
        );
    }

    if (written < 0 || (size_t)written >= sizeof(formatted) ||
        (size_t)written >= value_size) {
        errno = ENOSPC;
        return -1;
    }

    memcpy(value, formatted, (size_t)written + 1);
    return 0;
}

/* Print a sample to the terminal and write it to the CSV file. */
static int save_and_print(
    FILE *csv,
    const char *timestamp,
    const sensor_t *sensor,
    const char *value,
    sensor_name_mode_t name_mode
)
{
    char identifier[VALUE_BUFFER_SIZE];
    if (format_sensor_identifier(
            identifier,
            sizeof(identifier),
            sensor,
            name_mode
        ) != 0) {
        return -1;
    }

    /* Align short sensor names and long OIDs in terminal output. */
    printf("%-78s %s\n", identifier, value);

    if (csv_write_field(csv, timestamp) != 0 ||
        fputc(',', csv) == EOF ||
        csv_write_field(csv, sensor->simple_name) != 0 ||
        fputc(',', csv) == EOF ||
        csv_write_field(csv, sensor->oid_text) != 0 ||
        fputc(',', csv) == EOF ||
        csv_write_field(csv, value) != 0 ||
        fputc('\n', csv) == EOF) {
        return -1;
    }

    if (fflush(csv) != 0 || fflush(stdout) != 0) {
        return -1;
    }
    return 0;
}

/* Record an error row for every sensor when an SNMP cycle fails. */
static int record_cycle_error(
    FILE *csv,
    const char *timestamp,
    const char *error,
    sensor_name_mode_t name_mode
)
{
    for (size_t index = 0; index < SENSOR_COUNT; index++) {
        char value[VALUE_BUFFER_SIZE];
        int written = snprintf(value, sizeof(value), "ERROR: %s", error);
        if (written < 0 || (size_t)written >= sizeof(value)) {
            errno = ENOSPC;
            return -1;
        }
        if (save_and_print(
                csv,
                timestamp,
                &sensors[index],
                value,
                name_mode
            ) != 0) {
            return -1;
        }
    }
    return 0;
}

static int parse_ups_power_mode(
    const netsnmp_variable_list *variable,
    ups_power_mode_t *mode
)
{
    if (variable == NULL || variable->type != ASN_INTEGER ||
        variable->val.integer == NULL || mode == NULL) {
        return -1;
    }

    switch (*variable->val.integer) {
    case UPS_MODE_NO_POWER:
    case UPS_MODE_BYPASS:
    case UPS_MODE_MAINS:
    case UPS_MODE_BATTERY:
    case UPS_MODE_MAINS_ECO:
    case UPS_MODE_BATTERY_ECO:
    case UPS_MODE_UNKNOWN:
        *mode = (ups_power_mode_t)*variable->val.integer;
        return 0;
    default:
        return -1;
    }
}

static ups_index_t find_ups_power_mode_index(const sensor_t *sensor)
{
    if (strcmp(sensor->oid_text, UPS1_POWER_MODE_OID) == 0) {
        return UPS_1_INDEX;
    }
    if (strcmp(sensor->oid_text, UPS2_POWER_MODE_OID) == 0) {
        return UPS_2_INDEX;
    }
    return UPS_INDEX_INVALID;
}

static void update_ups_battery_alarm(
    const ups_power_mode_t modes[UPS_COUNT],
    const int mode_valid[UPS_COUNT],
    int *alarm_active
)
{
    int ups1_on_battery =
        mode_valid[UPS_1_INDEX] && modes[UPS_1_INDEX] == UPS_MODE_BATTERY;
    int ups2_on_battery =
        mode_valid[UPS_2_INDEX] && modes[UPS_2_INDEX] == UPS_MODE_BATTERY;

    if (ups1_on_battery || ups2_on_battery) {
        if (!*alarm_active) {
            fprintf(stderr, "ALERT: UPS in battery mode (4): ");
            if (ups1_on_battery && ups2_on_battery) {
                fprintf(stderr, "UPS 1 and UPS 2");
            } else if (ups1_on_battery) {
                fprintf(stderr, "UPS 1");
            } else {
                fprintf(stderr, "UPS 2");
            }
            fputc('\n', stderr);
        }
        *alarm_active = 1;
    } else if (mode_valid[UPS_1_INDEX] &&
               mode_valid[UPS_2_INDEX] &&
               modes[UPS_1_INDEX] != UPS_MODE_UNKNOWN &&
               modes[UPS_2_INDEX] != UPS_MODE_UNKNOWN) {
        if (*alarm_active) {
            fputs("RECOVERY: no UPS reports battery mode (4).\n",
                  stderr);
        }
        *alarm_active = 0;
    }
}

static void format_sensor_value(
    char *buffer,
    size_t buffer_size,
    const sensor_t *sensor,
    const netsnmp_variable_list *variable
)
{
    int written = snprint_value(
        buffer,
        buffer_size,
        variable->name,
        variable->name_length,
        variable
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        snprintf(buffer, buffer_size, "Value too long");
        return;
    }

    for (char *character = buffer; *character != '\0'; character++) {
        if (*character == '\n' || *character == '\r') {
            *character = ' ';
        }
    }

    if (append_sensor_unit(buffer, buffer_size, sensor, variable) != 0) {
        snprintf(buffer, buffer_size, "Value too long");
    }
}

/* Query all configured OIDs and record their values or errors. */
static int poll_sensors(
    netsnmp_session *session,
    FILE *csv,
    const char *host,
    unsigned int cycle,
    sensor_name_mode_t name_mode,
    int *battery_alarm_active
)
{
    char timestamp[64];
    /* Use one timestamp for every value in this polling cycle. */
    if (format_timestamp(timestamp, sizeof(timestamp)) != 0) {
        return -1;
    }

    printf("\n[%s] Cycle %u - Host: %s\n", timestamp, cycle, host);
    const char *identifier_header;
    switch (name_mode) {
    case SENSOR_NAME_SIMPLE:
        identifier_header = "SENSOR";
        break;
    case SENSOR_NAME_MIB:
        identifier_header = "MIB OID";
        break;
    case SENSOR_NAME_BOTH:
        identifier_header = "SENSOR | MIB OID";
        break;
    default:
        identifier_header = "SENSOR | MIB OID";
        break;
    }
    printf("%-78s %s\n", identifier_header, "VALUE");
    printf("----------------------------------------------------------------------------------------------\n");

    /* Query all OIDs together in one SNMP GET request. */
    netsnmp_pdu *request = snmp_pdu_create(SNMP_MSG_GET);
    if (request == NULL) {
        errno = ENOMEM;
        return -1;
    }

    /* Add each OID without a value; the device supplies the response values. */
    for (size_t index = 0; index < SENSOR_COUNT; index++) {
        if (snmp_add_null_var(
                request,
                sensors[index].oid_value,
                sensors[index].oid_length
            ) == NULL) {
            snmp_free_pdu(request);
            errno = ENOMEM;
            return -1;
        }
    }

    netsnmp_pdu *response = NULL;
    int status = snmp_synch_response(session, request, &response);
    /* Record transport and SNMP errors explicitly in the CSV. */
    if (status != STAT_SUCCESS || response == NULL ||
        response->errstat != SNMP_ERR_NOERROR) {
        const char *error = "SNMP communication error";
        if (status == STAT_TIMEOUT) {
            error = "SNMP timeout";
        } else if (status == STAT_SUCCESS && response != NULL) {
            error = snmp_errstring(response->errstat);
        }
        int result = record_cycle_error(csv, timestamp, error, name_mode);
        if (response != NULL) {
            snmp_free_pdu(response);
        }
        return result;
    }

    /* Net-SNMP returns variables in the same order as the request. */
    netsnmp_variable_list *variable = response->variables;
    ups_power_mode_t ups_modes[UPS_COUNT] = {
        UPS_MODE_UNKNOWN,
        UPS_MODE_UNKNOWN
    };
    int ups_mode_valid[UPS_COUNT] = {0, 0};
    for (size_t index = 0; index < SENSOR_COUNT; index++) {
        char value[VALUE_BUFFER_SIZE];

        if (variable == NULL) {
            snprintf(value, sizeof(value), "OID missing from SNMP response");
        } else {
            format_sensor_value(
                value,
                sizeof(value),
                &sensors[index],
                variable
            );
            ups_index_t ups_index =
                find_ups_power_mode_index(&sensors[index]);
            if (ups_index != UPS_INDEX_INVALID &&
                parse_ups_power_mode(variable, &ups_modes[ups_index]) == 0) {
                ups_mode_valid[ups_index] = 1;
            }
            variable = variable->next_variable;
        }

        if (save_and_print(
                csv,
                timestamp,
                &sensors[index],
                value,
                name_mode
            ) != 0) {
            snmp_free_pdu(response);
            return -1;
        }
    }

    update_ups_battery_alarm(
        ups_modes,
        ups_mode_valid,
        battery_alarm_active
    );

    snmp_free_pdu(response);
    return 0;
}

/* Compare two timespec values chronologically. */
static int compare_timespec(
    const struct timespec *left,
    const struct timespec *right
)
{
    if (left->tv_sec != right->tv_sec) {
        return left->tv_sec < right->tv_sec ? -1 : 1;
    }
    if (left->tv_nsec != right->tv_nsec) {
        return left->tv_nsec < right->tv_nsec ? -1 : 1;
    }
    return 0;
}

/* Wait until a deadline measured with CLOCK_MONOTONIC. */
static int sleep_until(const struct timespec *deadline)
{
    int result;
    /* Usa una scadenza assoluta e riprova se la chiamata viene interrotta. */
    do {
        result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, deadline, NULL);
    } while (result == EINTR);

    if (result != 0) {
        errno = result;
        return -1;
    }
    return 0;
}

/* Parse options, open resources, collect samples, and close resources. */
int main(int argc, char **argv)
{
    monitor_options_t options = {
        .host = DEFAULT_HOST,
        .mib_dir = NULL,
        .csv_path = NULL,
        .duration = DEFAULT_DURATION_SECONDS,
        .interval = DEFAULT_INTERVAL_SECONDS,
        .name_mode = SENSOR_NAME_SIMPLE
    };

    options_result_t option_result = parse_options(argc, argv, &options);
    if (option_result == OPTIONS_HELP) {
        return EXIT_SUCCESS;
    }
    if (option_result == OPTIONS_ERROR) {
        return EXIT_FAILURE;
    }

    const char *host = options.host;
    const char *mib_dir = options.mib_dir;
    const char *csv_path = options.csv_path;
    unsigned int duration = options.duration;
    unsigned int interval = options.interval;
    sensor_name_mode_t name_mode = options.name_mode;

    /* Keep the SNMP community out of source code; read it from the environment. */
    const char *community = getenv("ECC_COMMUNITY");
    if (community == NULL || community[0] == '\0') {
        fprintf(stderr, "Set the community in the ECC_COMMUNITY variable.\n");
        return EXIT_FAILURE;
    }

    /* Default to a timestamped CSV in output and MIB files in $HOME/MIB. */
    char default_csv_path[MAX_PATH_LENGTH];
    if (csv_path == NULL) {
        if (make_default_csv_path(
                default_csv_path,
                sizeof(default_csv_path)
            ) != 0) {
            perror("Creating the default CSV filename");
            return EXIT_FAILURE;
        }
        csv_path = default_csv_path;
    }

    char default_mib_dir[MAX_PATH_LENGTH];
    if (mib_dir == NULL) {
        const char *home = getenv("HOME");
        if (home == NULL) {
            fprintf(stderr, "HOME is not set; specify the MIB directory with -M.\n");
            return EXIT_FAILURE;
        }
        int written = snprintf(
            default_mib_dir,
            sizeof(default_mib_dir),
            "%s/MIB",
            home
        );
        if (written < 0 || (size_t)written >= sizeof(default_mib_dir)) {
            fprintf(stderr, "MIB directory path is too long.\n");
            return EXIT_FAILURE;
        }
        mib_dir = default_mib_dir;
    }

    /* Load MIB names and initialize Net-SNMP before sending requests. */
    if (snmp_oid_init(mib_dir) != 0) {
        perror("Failed to load MIB files");
        return EXIT_FAILURE;
    }
    if (netsnmp_ds_set_boolean(
            NETSNMP_DS_LIBRARY_ID,
            NETSNMP_DS_LIB_DONT_PERSIST_STATE,
            1
        ) != SNMPERR_SUCCESS) {
        fprintf(stderr, "Could not initialize Net-SNMP settings.\n");
        return EXIT_FAILURE;
    }
    init_snmp("ecc-monitor");

    /* Resolve symbolic MIB names to numeric OIDs before opening the session. */
    for (size_t index = 0; index < SENSOR_COUNT; index++) {
        sensors[index].oid_length = MAX_OID_LEN;
        if (snmp_parse_oid(
                sensors[index].oid_text,
                sensors[index].oid_value,
                &sensors[index].oid_length
            ) == NULL) {
            fprintf(
                stderr,
                "OID not found in MIB: %s\n",
                sensors[index].oid_text
            );
            snmp_shutdown("ecc-monitor");
            return EXIT_FAILURE;
        }
    }

    /* Configure SNMPv2c with a one-second timeout and no retries. */
    netsnmp_session session;
    snmp_sess_init(&session);
    session.peername = (char *)host;
    session.version = SNMP_VERSION_2c;
    session.community = (u_char *)community;
    session.community_len = strlen(community);
    session.timeout = DEFAULT_TIMEOUT_MICROSECONDS;
    session.retries = 0;

    netsnmp_session *snmp_session = snmp_open(&session);
    if (snmp_session == NULL) {
        snmp_perror("Opening SNMP session");
        snmp_shutdown("ecc-monitor");
        return EXIT_FAILURE;
    }

    /* Create the CSV and write its header before collecting samples. */
    FILE *csv = fopen(csv_path, "w");
    if (csv == NULL) {
        perror(csv_path);
        snmp_close(snmp_session);
        snmp_shutdown("ecc-monitor");
        return EXIT_FAILURE;
    }
    if (fputs("\"timestamp\",\"sensor\",\"oid\",\"value\"\n", csv) == EOF) {
        perror("Writing CSV header");
        fclose(csv);
        snmp_close(snmp_session);
        snmp_shutdown("ecc-monitor");
        return EXIT_FAILURE;
    }

    printf(
        "Collecting for %u s, interval %u s. CSV: %s\n",
        duration,
        interval,
        csv_path
    );

    /* CLOCK_MONOTONIC measures duration without wall-clock adjustments. */
    struct timespec start;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        fclose(csv);
        snmp_close(snmp_session);
        snmp_shutdown("ecc-monitor");
        return EXIT_FAILURE;
    }
    struct timespec finish = start;
    finish.tv_sec += duration;
    struct timespec next_poll = start;
    unsigned int cycle = 0;
    int battery_alarm_active = 0;
    int exit_status = EXIT_SUCCESS;

    /* Continue while another polling cycle can start before the deadline. */
    while (compare_timespec(&next_poll, &finish) < 0) {
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            perror("clock_gettime");
            exit_status = EXIT_FAILURE;
            break;
        }
        if (compare_timespec(&now, &finish) >= 0) {
            break;
        }

        /* Query all OIDs and record the results for this cycle. */
        cycle++;
        if (poll_sensors(
                snmp_session,
                csv,
                host,
                cycle,
                name_mode,
                &battery_alarm_active
            ) != 0) {
            perror("Collecting or saving sample data");
            exit_status = EXIT_FAILURE;
            break;
        }

        /* Advance from the scheduled polling time, not the current time. */
        next_poll.tv_sec += interval;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            perror("clock_gettime");
            exit_status = EXIT_FAILURE;
            break;
        }
        /* Skip missed intervals if a cycle took longer than expected. */
        while (compare_timespec(&next_poll, &now) < 0) {
            next_poll.tv_sec += interval;
        }
        /* Do not sleep past the collection deadline. */
        if (compare_timespec(&next_poll, &finish) < 0 &&
            sleep_until(&next_poll) != 0) {
            perror("Waiting between samples");
            exit_status = EXIT_FAILURE;
            break;
        }
    }

    /* Close the CSV and SNMP session after collection or a cycle error. */
    if (fclose(csv) != 0) {
        perror("Closing CSV file");
        exit_status = EXIT_FAILURE;
    }
    snmp_close(snmp_session);
    snmp_shutdown("ecc-monitor");
    printf(
        "\nCollection finished: %u cycles recorded in %s\n",
        cycle,
        csv_path
    );
    return exit_status;
}
