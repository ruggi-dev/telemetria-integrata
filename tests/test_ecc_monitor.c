#define main ecc_monitor_program_main
#include "../src/ecc_monitor.c"
#undef main

#include <arpa/inet.h>
#include <netinet/in.h>
#include <regex.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", \
                    __FILE__, __LINE__, #condition); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

typedef struct {
    FILE *stream;
    int saved_fd;
    int target_fd;
} output_capture_t;

static void capture_start(output_capture_t *capture, int target_fd)
{
    fflush(target_fd == STDOUT_FILENO ? stdout : stderr);
    capture->stream = tmpfile();
    CHECK(capture->stream != NULL);

    capture->saved_fd = dup(target_fd);
    CHECK(capture->saved_fd >= 0);
    capture->target_fd = target_fd;
    CHECK(dup2(fileno(capture->stream), target_fd) >= 0);
}

static size_t capture_finish(
    output_capture_t *capture,
    char *buffer,
    size_t buffer_size
)
{
    fflush(capture->target_fd == STDOUT_FILENO ? stdout : stderr);
    CHECK(dup2(capture->saved_fd, capture->target_fd) >= 0);
    CHECK(close(capture->saved_fd) == 0);

    CHECK(fseek(capture->stream, 0, SEEK_SET) == 0);
    size_t length = fread(buffer, 1, buffer_size - 1, capture->stream);
    CHECK(!ferror(capture->stream));
    buffer[length] = '\0';
    CHECK(fclose(capture->stream) == 0);
    return length;
}

static size_t read_stream(FILE *stream, char *buffer, size_t buffer_size)
{
    CHECK(fflush(stream) == 0);
    CHECK(fseek(stream, 0, SEEK_SET) == 0);
    size_t length = fread(buffer, 1, buffer_size - 1, stream);
    CHECK(!ferror(stream));
    buffer[length] = '\0';
    return length;
}

static size_t count_newlines(const char *text)
{
    size_t count = 0;
    for (const char *character = text; *character != '\0'; character++) {
        if (*character == '\n') {
            count++;
        }
    }
    return count;
}

static void check_regex(const char *pattern, const char *text)
{
    regex_t regex;
    CHECK(regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB) == 0);
    int result = regexec(&regex, text, 0, NULL, 0);
    regfree(&regex);
    CHECK(result == 0);
}

static void test_parse_positive_seconds(void)
{
    unsigned int value = 0;

    CHECK(parse_positive_seconds("20", &value) == 0);
    CHECK(value == 20);
    CHECK(parse_positive_seconds("1", &value) == 0);
    CHECK(value == 1);
    CHECK(parse_positive_seconds("0", &value) == -1);
    CHECK(parse_positive_seconds("3601", &value) == -1);
    CHECK(parse_positive_seconds("20s", &value) == -1);
    CHECK(parse_positive_seconds("abc", &value) == -1);
}

static void test_timestamp_formatters(void)
{
    char timestamp[64];
    char path[MAX_PATH_LENGTH];

    CHECK(format_timestamp(timestamp, sizeof(timestamp)) == 0);
    check_regex(
        "^[0-9]{4}-[0-9]{2}-[0-9]{2} "
        "[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}$",
        timestamp
    );
    CHECK(make_default_csv_path(path, sizeof(path)) == 0);
    check_regex(
        "^output/ecc-monitor_[0-9]{4}-[0-9]{2}-[0-9]{2}_"
        "[0-9]{2}-[0-9]{2}-[0-9]{2}\\.csv$",
        path
    );

    errno = 0;
    CHECK(format_timestamp(timestamp, 1) == -1);
    CHECK(errno == ENOSPC);
    errno = 0;
    CHECK(make_default_csv_path(path, 1) == -1);
    CHECK(errno == ENOSPC);
}

static void test_csv_write_field(void)
{
    FILE *csv = tmpfile();
    CHECK(csv != NULL);

    const char *field = "sensor,\"value\"\nnext";
    const char *expected = "\"sensor,\"\"value\"\"\nnext\"";
    CHECK(csv_write_field(csv, field) == 0);

    char result[128];
    read_stream(csv, result, sizeof(result));
    CHECK(strcmp(result, expected) == 0);
    CHECK(fclose(csv) == 0);
}

static void test_save_and_print(void)
{
    FILE *csv = tmpfile();
    CHECK(csv != NULL);
    sensor_t sensor = {
        .simple_name = "Sensore di prova",
        .oid_text = "TEST-MIB::testSensor.0"
    };
    output_capture_t capture;
    char terminal_output[512];
    char csv_output[512];

    capture_start(&capture, STDOUT_FILENO);
    CHECK(save_and_print(csv, "2026-01-02 03:04:05.006",
                         &sensor, "INTEGER: 42", SENSOR_NAME_BOTH) == 0);
    capture_finish(&capture, terminal_output, sizeof(terminal_output));

    CHECK(strstr(terminal_output, sensor.simple_name) != NULL);
    CHECK(strstr(terminal_output, sensor.oid_text) != NULL);
    CHECK(strstr(terminal_output, "INTEGER: 42") != NULL);
    CHECK(strstr(terminal_output, "GRUPPO") == NULL);
    read_stream(csv, csv_output, sizeof(csv_output));
    CHECK(strcmp(
        csv_output,
        "\"2026-01-02 03:04:05.006\","
        "\"Sensore di prova\",\"TEST-MIB::testSensor.0\",\"INTEGER: 42\"\n"
    ) == 0);
    CHECK(fclose(csv) == 0);
}

static void test_sensor_name_modes(void)
{
    sensor_name_mode_t mode = SENSOR_NAME_SIMPLE;
    char identifier[VALUE_BUFFER_SIZE];

    CHECK(parse_sensor_name_mode("semplice", &mode) == 0);
    CHECK(mode == SENSOR_NAME_SIMPLE);
    CHECK(parse_sensor_name_mode("mib", &mode) == 0);
    CHECK(mode == SENSOR_NAME_MIB);
    CHECK(parse_sensor_name_mode("entrambi", &mode) == 0);
    CHECK(mode == SENSOR_NAME_BOTH);
    CHECK(parse_sensor_name_mode("altro", &mode) == -1);

    CHECK(format_sensor_identifier(
        identifier, sizeof(identifier), &sensors[0], SENSOR_NAME_SIMPLE
    ) == 0);
    CHECK(strcmp(identifier, sensors[0].simple_name) == 0);

    CHECK(format_sensor_identifier(
        identifier, sizeof(identifier), &sensors[0], SENSOR_NAME_MIB
    ) == 0);
    CHECK(strcmp(identifier, sensors[0].oid_text) == 0);

    CHECK(format_sensor_identifier(
        identifier, sizeof(identifier), &sensors[0], SENSOR_NAME_BOTH
    ) == 0);
    CHECK(strstr(identifier, sensors[0].simple_name) != NULL);
    CHECK(strstr(identifier, sensors[0].oid_text) != NULL);
}

static void test_sensor_unit_conversion(void)
{
    char value[VALUE_BUFFER_SIZE];
    long raw_value = 164;
    netsnmp_variable_list variable = {
        .type = ASN_INTEGER,
        .val.integer = &raw_value
    };

    snprintf(value, sizeof(value), "INTEGER: %ld", raw_value);
    CHECK(append_sensor_unit(value, sizeof(value), &sensors[0], &variable) == 0);
    CHECK(strcmp(value, "INTEGER: 164 (16.4 °C)") == 0);

    raw_value = 456;
    variable.type = ASN_UNSIGNED;
    snprintf(value, sizeof(value), "Gauge32: %ld", raw_value);
    CHECK(append_sensor_unit(value, sizeof(value), &sensors[10], &variable) == 0);
    CHECK(strcmp(value, "Gauge32: 456 (45.6 %RH)") == 0);

    raw_value = 10;
    variable.type = ASN_INTEGER;
    snprintf(value, sizeof(value), "INTEGER: %ld", raw_value);
    CHECK(append_sensor_unit(value, sizeof(value), &sensors[12], &variable) == 0);
    CHECK(strcmp(value, "INTEGER: 10 (1.0 kW)") == 0);

    raw_value = 111;
    variable.type = ASN_UNSIGNED;
    snprintf(value, sizeof(value), "Gauge32: %ld", raw_value);
    CHECK(append_sensor_unit(value, sizeof(value), &sensors[14], &variable) == 0);
    CHECK(strcmp(value, "Gauge32: 111 (11.1 %)") == 0);

    raw_value = 468;
    variable.type = ASN_INTEGER;
    snprintf(value, sizeof(value), "INTEGER: %ld", raw_value);
    CHECK(append_sensor_unit(value, sizeof(value), &sensors[16], &variable) == 0);
    CHECK(strcmp(value, "INTEGER: 468 (46.8 W)") == 0);

    raw_value = 123;
    variable.type = ASN_UNSIGNED;
    snprintf(value, sizeof(value), "Gauge32: %ld", raw_value);
    CHECK(append_sensor_unit(value, sizeof(value), &sensors[6], &variable) == 0);
    CHECK(strcmp(value, "Gauge32: 123 (123 W)") == 0);

    raw_value = -5;
    variable.type = ASN_INTEGER;
    snprintf(value, sizeof(value), "INTEGER: %ld", raw_value);
    CHECK(append_sensor_unit(value, sizeof(value), &sensors[8], &variable) == 0);
    CHECK(strcmp(value, "INTEGER: -5 (-0.5 °C)") == 0);
}

static void test_record_cycle_error(void)
{
    FILE *csv = tmpfile();
    CHECK(csv != NULL);
    output_capture_t capture;
    char terminal_output[8192];
    char csv_output[8192];

    capture_start(&capture, STDOUT_FILENO);
    CHECK(record_cycle_error(csv, "2026-01-02 03:04:05.006",
                             "timeout SNMP", SENSOR_NAME_SIMPLE) == 0);
    capture_finish(&capture, terminal_output, sizeof(terminal_output));
    read_stream(csv, csv_output, sizeof(csv_output));

    CHECK(count_newlines(csv_output) == SENSOR_COUNT);
    CHECK(strstr(csv_output, "\"ERRORE: timeout SNMP\"") != NULL);
    CHECK(strstr(terminal_output, "ERRORE: timeout SNMP") != NULL);
    CHECK(strstr(
        terminal_output,
        sensors[SENSOR_COUNT - 1].simple_name
    ) != NULL);
    CHECK(fclose(csv) == 0);
}

static void test_timespec_helpers(void)
{
    struct timespec earlier = {.tv_sec = 10, .tv_nsec = 100};
    struct timespec later = {.tv_sec = 10, .tv_nsec = 200};

    CHECK(compare_timespec(&earlier, &later) < 0);
    CHECK(compare_timespec(&later, &earlier) > 0);
    CHECK(compare_timespec(&earlier, &earlier) == 0);

    struct timespec deadline;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &deadline) == 0);
    deadline.tv_nsec += 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    CHECK(sleep_until(&deadline) == 0);
    struct timespec now;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    CHECK(compare_timespec(&now, &deadline) >= 0);
}

static void test_program_main(void)
{
    char *help_arguments[] = {"ecc-monitor", "-h", NULL};
    output_capture_t capture;
    char output[1024];

    optind = 1;
    opterr = 0;
    capture_start(&capture, STDOUT_FILENO);
    CHECK(ecc_monitor_program_main(2, help_arguments) == EXIT_SUCCESS);
    capture_finish(&capture, output, sizeof(output));
    CHECK(strstr(output, "Uso: ecc-monitor") != NULL);
    CHECK(strstr(output, "ECC_COMMUNITY") != NULL);
    CHECK(strstr(output, "-n semplice|mib|entrambi") != NULL);

    const char *existing_community = getenv("ECC_COMMUNITY");
    char *saved_community = existing_community == NULL
        ? NULL
        : strdup(existing_community);
    CHECK(existing_community == NULL || saved_community != NULL);
    CHECK(unsetenv("ECC_COMMUNITY") == 0);

    char *arguments[] = {"ecc-monitor", NULL};
    char error_output[512];
    optind = 1;
    capture_start(&capture, STDERR_FILENO);
    int result = ecc_monitor_program_main(1, arguments);
    capture_finish(&capture, error_output, sizeof(error_output));

    if (saved_community != NULL) {
        CHECK(setenv("ECC_COMMUNITY", saved_community, 1) == 0);
        free(saved_community);
    }
    CHECK(result == EXIT_FAILURE);
    CHECK(strstr(error_output, "ECC_COMMUNITY") != NULL);
}

static void get_test_mib_directory(char *buffer, size_t buffer_size)
{
    const char *configured = getenv("ECC_MIB_DIR");
    if (configured != NULL && configured[0] != '\0') {
        int written = snprintf(buffer, buffer_size, "%s", configured);
        CHECK(written >= 0 && (size_t)written < buffer_size);
        return;
    }

    const char *home = getenv("HOME");
    CHECK(home != NULL);
    int written = snprintf(buffer, buffer_size, "%s/MIB", home);
    CHECK(written >= 0 && (size_t)written < buffer_size);
}

static void test_snmp_oid_helpers(void)
{
    const char *numeric =
        ".1.3.6.1.4.1.2011.6.178.1.1.25.1.1.4.4123";
    const char *symbolic =
        "HUAWEI-ECC-MIB::hwTempHumTempSensor.4123";
    char output[512];
    char mib_dir[MAX_PATH_LENGTH];

    errno = 0;
    CHECK(snmp_oid_translate(numeric, SNMP_OID_TO_SYMBOLIC,
                             output, sizeof(output)) == -1);
    CHECK(errno == EPERM);

    errno = 0;
    CHECK(snmp_oid_init("/path/that/does/not/exist") == -1);
    CHECK(errno == ENOENT);
    get_test_mib_directory(mib_dir, sizeof(mib_dir));
    CHECK(snmp_oid_init(mib_dir) == 0);

    CHECK(snmp_oid_translate(numeric, SNMP_OID_TO_SYMBOLIC,
                             output, sizeof(output)) == 0);
    CHECK(strcmp(output, symbolic) == 0);
    CHECK(snmp_oid_translate(symbolic, SNMP_OID_TO_NUMERIC,
                             output, sizeof(output)) == 0);
    CHECK(strcmp(output, numeric) == 0);

    output_capture_t capture;
    char error_output[512];
    errno = 0;
    capture_start(&capture, STDERR_FILENO);
    int invalid_oid_result = snmp_oid_translate(
        "not-an-oid",
        SNMP_OID_TO_NUMERIC,
        output,
        sizeof(output)
    );
    capture_finish(&capture, error_output, sizeof(error_output));
    CHECK(invalid_oid_result == -1);
    CHECK(errno == ENOENT);

    char small_buffer[2];
    errno = 0;
    CHECK(snmp_oid_translate(symbolic, SNMP_OID_TO_NUMERIC,
                             small_buffer, sizeof(small_buffer)) == -1);
    CHECK(errno == ENOSPC);

    errno = 0;
    CHECK(snmp_oid_init(mib_dir) == -1);
    CHECK(errno == EALREADY);
}

static void test_sensor_oid_catalog(void)
{
    for (size_t index = 0; index < SENSOR_COUNT; index++) {
        sensors[index].oid_length = MAX_OID_LEN;
        CHECK(snmp_parse_oid(
            sensors[index].oid_text,
            sensors[index].oid_value,
            &sensors[index].oid_length
        ) != NULL);
        CHECK(sensors[index].oid_length > 0);
    }
}

static void test_poll_sensors_timeout(void)
{
    int sink = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(sink >= 0);

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);
    CHECK(bind(sink, (struct sockaddr *)&address, sizeof(address)) == 0);

    socklen_t address_length = sizeof(address);
    CHECK(getsockname(sink, (struct sockaddr *)&address, &address_length) == 0);

    char peer_name[64];
    int written = snprintf(
        peer_name,
        sizeof(peer_name),
        "udp:127.0.0.1:%u",
        (unsigned int)ntohs(address.sin_port)
    );
    CHECK(written >= 0 && (size_t)written < sizeof(peer_name));

    CHECK(netsnmp_ds_set_boolean(
        NETSNMP_DS_LIBRARY_ID,
        NETSNMP_DS_LIB_DONT_PERSIST_STATE,
        1
    ) == SNMPERR_SUCCESS);
    init_snmp("ecc-monitor-tests");

    netsnmp_session session;
    snmp_sess_init(&session);
    session.peername = peer_name;
    session.version = SNMP_VERSION_2c;
    session.community = (u_char *)"test-community";
    session.community_len = strlen("test-community");
    session.timeout = 100000L;
    session.retries = 0;

    netsnmp_session *snmp_session = snmp_open(&session);
    CHECK(snmp_session != NULL);

    FILE *csv = tmpfile();
    CHECK(csv != NULL);
    output_capture_t capture;
    char terminal_output[8192];
    char csv_output[8192];

    capture_start(&capture, STDOUT_FILENO);
    CHECK(poll_sensors(
        snmp_session,
        csv,
        "127.0.0.1",
        1,
        SENSOR_NAME_SIMPLE
    ) == 0);
    capture_finish(&capture, terminal_output, sizeof(terminal_output));
    read_stream(csv, csv_output, sizeof(csv_output));

    CHECK(strstr(terminal_output, "Ciclo 1") != NULL);
    CHECK(strstr(terminal_output, "ERRORE: timeout SNMP") != NULL);
    CHECK(count_newlines(csv_output) == SENSOR_COUNT);

    CHECK(fclose(csv) == 0);
    snmp_close(snmp_session);
    snmp_shutdown("ecc-monitor-tests");
    CHECK(close(sink) == 0);
}

int main(void)
{
    test_program_main();
    puts("PASS test_program_main");

    test_parse_positive_seconds();
    puts("PASS test_parse_positive_seconds");

    test_timestamp_formatters();
    puts("PASS test_timestamp_formatters");

    test_csv_write_field();
    puts("PASS test_csv_write_field");

    test_save_and_print();
    puts("PASS test_save_and_print");

    test_sensor_name_modes();
    puts("PASS test_sensor_name_modes");

    test_sensor_unit_conversion();
    puts("PASS test_sensor_unit_conversion");

    test_record_cycle_error();
    puts("PASS test_record_cycle_error");

    test_timespec_helpers();
    puts("PASS test_timespec_helpers");

    test_snmp_oid_helpers();
    puts("PASS test_snmp_oid_helpers");

    test_sensor_oid_catalog();
    puts("PASS test_sensor_oid_catalog");

    test_poll_sensors_timeout();
    puts("PASS test_poll_sensors_timeout");

    puts("Tutti i test sono stati superati.");
    return EXIT_SUCCESS;
}
