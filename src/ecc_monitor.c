#include <errno.h>
#include <getopt.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "snmp_oid.h"

/* Valori usati quando non vengono specificate opzioni da riga di comando. */
#define DEFAULT_HOST "172.29.2.30"
#define DEFAULT_DURATION_SECONDS 20
#define DEFAULT_INTERVAL_SECONDS 1
#define DEFAULT_TIMEOUT_MICROSECONDS 1000000L
#define DEFAULT_CSV_DIRECTORY "output"
#define MAX_PATH_LENGTH 4096
#define VALUE_BUFFER_SIZE 512

typedef enum {
    SENSOR_NAME_SIMPLE,
    SENSOR_NAME_MIB,
    SENSOR_NAME_BOTH
} sensor_name_mode_t;

/* Conserva nome leggibile, OID e unità/scala del valore del sensore. */
typedef struct {
    const char *simple_name;
    const char *oid_text;
    oid oid_value[MAX_OID_LEN];
    size_t oid_length;
    unsigned int scale_factor;
    const char *unit;
} sensor_t;

/* Sensori interrogati a ogni ciclo; l'ordine qui determina quello dell'output. */
static sensor_t sensors[] = {
    {
        .simple_name = "Aria fredda di mandata - Condizionatore 1",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010AverageSupplyAirTemperature.4121",
        .scale_factor = 10, .unit = "°C"
    },
    {
        .simple_name = "Aria fredda di mandata - Condizionatore 2",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010AverageSupplyAirTemperature.4122",
        .scale_factor = 10, .unit = "°C"
    },
    {
        .simple_name = "Aria calda di ritorno - Condizionatore 1",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010AverageReturnAirTemperature.4121",
        .scale_factor = 10, .unit = "°C"
    },
    {
        .simple_name = "Aria calda di ritorno - Condizionatore 2",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010AverageReturnAirTemperature.4122",
        .scale_factor = 10, .unit = "°C"
    },
    {
        .simple_name = "Temperatura corridoio freddo - Condizionatore 1",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010AverageColdAisleTemperature.4121",
        .scale_factor = 10, .unit = "°C"
    },
    {
        .simple_name = "Temperatura corridoio freddo - Condizionatore 2",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010AverageColdAisleTemperature.4122",
        .scale_factor = 10, .unit = "°C"
    },
    {
        .simple_name = "Assorbimento PSU - Condizionatore 1",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010PsuInputPower.4121",
        .scale_factor = 1, .unit = "W"
    },
    {
        .simple_name = "Assorbimento PSU - Condizionatore 2",
        .oid_text = "HUAWEI-ECC-MIB::hwNetCol5000A010PsuInputPower.4122",
        .scale_factor = 1, .unit = "W"
    },
    {
        .simple_name = "Temperatura zona calda - Sonda T/H 1",
        .oid_text = "HUAWEI-ECC-MIB::hwTempHumTempSensor.4123",
        .scale_factor = 10, .unit = "°C"
    },
    {
        .simple_name = "Temperatura corridoio freddo - Sonda T/H 2",
        .oid_text = "HUAWEI-ECC-MIB::hwTempHumTempSensor.4124",
        .scale_factor = 10, .unit = "°C"
    },
    {
        .simple_name = "Umidita relativa zona calda - Sonda T/H 1",
        .oid_text = "HUAWEI-ECC-MIB::hwTempHumHumSensor.4123",
        .scale_factor = 10, .unit = "%RH"
    },
    {
        .simple_name = "Umidita relativa corridoio freddo - Sonda T/H 2",
        .oid_text = "HUAWEI-ECC-MIB::hwTempHumHumSensor.4124",
        .scale_factor = 10, .unit = "%RH"
    },
    {
        .simple_name = "Potenza erogata ai server - UPS 1",
        .oid_text = "HUAWEI-ECC-MIB::hwUPS2000GAOutputActivePower.4118",
        .scale_factor = 10, .unit = "kW"
    },
    {
        .simple_name = "Potenza erogata ai server - UPS 2",
        .oid_text = "HUAWEI-ECC-MIB::hwUPS2000GAOutputActivePower.4119",
        .scale_factor = 10, .unit = "kW"
    },
    {
        .simple_name = "Carico UPS 1",
        .oid_text = "HUAWEI-ECC-MIB::hwUPS2000GALoadRatio.4118",
        .scale_factor = 10, .unit = "%"
    },
    {
        .simple_name = "Carico UPS 2",
        .oid_text = "HUAWEI-ECC-MIB::hwUPS2000GALoadRatio.4119",
        .scale_factor = 10, .unit = "%"
    },
    {
        .simple_name = "Potenza DC - Raddrizzatore 1",
        .oid_text = "HUAWEI-ECC-MIB::hwPowerModuleDcOutputPower.111",
        .scale_factor = 10, .unit = "W"
    }
};

#define SENSOR_COUNT (sizeof(sensors) / sizeof(sensors[0]))

/* Mostra la sintassi delle opzioni e i valori predefiniti. */
static void print_usage(const char *program)
{
    printf(
        "Uso: %s [-H host] [-M directory_mib] [-o file_csv] "
        "[-d durata_secondi] [-i intervallo_secondi] "
        "[-n semplice|mib|entrambi]\n"
        "La community SNMPv2c va impostata nella variabile "
        "d'ambiente ECC_COMMUNITY.\n"
        "-n sceglie i nomi mostrati a terminale (default: semplice).\n"
        "Default: host %s, durata %d s, intervallo %d s, "
        "CSV %s/ecc-monitor_YYYY-MM-DD_HH-MM-SS.csv.\n",
        program,
        DEFAULT_HOST,
        DEFAULT_DURATION_SECONDS,
        DEFAULT_INTERVAL_SECONDS,
        DEFAULT_CSV_DIRECTORY
    );
}

/* Converte il valore dell'opzione -n nella modalità di visualizzazione. */
static int parse_sensor_name_mode(
    const char *text,
    sensor_name_mode_t *mode
)
{
    if (strcmp(text, "semplice") == 0) {
        *mode = SENSOR_NAME_SIMPLE;
    } else if (strcmp(text, "mib") == 0) {
        *mode = SENSOR_NAME_MIB;
    } else if (strcmp(text, "entrambi") == 0) {
        *mode = SENSOR_NAME_BOTH;
    } else {
        return -1;
    }
    return 0;
}

/* Prepara il nome del sensore da mostrare secondo la modalità scelta. */
static int format_sensor_identifier(
    char *buffer,
    size_t buffer_size,
    const sensor_t *sensor,
    sensor_name_mode_t mode
)
{
    int written;
    if (mode == SENSOR_NAME_SIMPLE) {
        written = snprintf(buffer, buffer_size, "%s", sensor->simple_name);
    } else if (mode == SENSOR_NAME_MIB) {
        written = snprintf(buffer, buffer_size, "%s", sensor->oid_text);
    } else {
        written = snprintf(
            buffer,
            buffer_size,
            "%s | %s",
            sensor->simple_name,
            sensor->oid_text
        );
    }

    if (written < 0 || (size_t)written >= buffer_size) {
        errno = ENOSPC;
        return -1;
    }
    return 0;
}

/* Convalida una durata in secondi e la converte in unsigned int. */
static int parse_positive_seconds(const char *text, unsigned int *value)
{
    char *end = NULL;
    errno = 0;
    /* strtoul consente di rilevare sia input non numerici sia overflow. */
    unsigned long parsed = strtoul(text, &end, 10);

    /* Rifiuta stringhe vuote, caratteri extra, zero e valori oltre un'ora. */
    if (errno != 0 || end == text || *end != '\0' ||
        parsed == 0 || parsed > 3600) {
        return -1;
    }

    *value = (unsigned int)parsed;
    return 0;
}

/* Formatta l'ora locale corrente con precisione al millisecondo. */
static int format_timestamp(char *buffer, size_t buffer_size)
{
    struct timespec now;
    struct tm local_time;
    char date[32];

    /* L'orologio reale serve per mostrare l'ora effettiva del campione. */
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return -1;
    }
    if (localtime_r(&now.tv_sec, &local_time) == NULL ||
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &local_time) == 0) {
        errno = EINVAL;
        return -1;
    }

    /* Aggiunge i millisecondi al timestamp leggibile. */
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

/* Genera il percorso CSV predefinito usando data e ora locali. */
static int make_default_csv_path(char *buffer, size_t buffer_size)
{
    struct timespec now;
    struct tm local_time;
    char date[32];

    /* Il nome del CSV usa la data locale, con precisione al secondo. */
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

/* Scrive un campo CSV tra virgolette, raddoppiando quelle interne. */
static int csv_write_field(FILE *csv, const char *field)
{
    if (fputc('"', csv) == EOF) {
        return -1;
    }

    /* In CSV una virgoletta interna si rappresenta raddoppiandola. */
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

/* Aggiunge al valore SNMP grezzo la conversione e l'unità dichiarata nel MIB. */
static int append_sensor_unit(
    char *value,
    size_t value_size,
    const sensor_t *sensor,
    const netsnmp_variable_list *variable
)
{
    if (sensor->unit == NULL || variable->val.integer == NULL ||
        (variable->type != ASN_INTEGER && variable->type != ASN_UNSIGNED)) {
        return 0;
    }

    char formatted[VALUE_BUFFER_SIZE];
    long raw_value = *variable->val.integer;
    int written;

    if (sensor->scale_factor == 10) {
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

/* Stampa un campione sul terminale e lo salva nel file CSV. */
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

    /* L'allineamento rende leggibili sia i nomi semplici sia gli OID lunghi. */
    printf("%-78s %s\n", identifier, value);

    /* Il CSV conserva sempre nome comprensibile e OID per identificare il dato. */
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

    /* Rende subito visibili i dati, anche mentre la raccolta è in corso. */
    if (fflush(csv) != 0 || fflush(stdout) != 0) {
        return -1;
    }
    return 0;
}

/* Registra un errore per ogni sensore quando fallisce un ciclo SNMP. */
static int record_cycle_error(
    FILE *csv,
    const char *timestamp,
    const char *error,
    sensor_name_mode_t name_mode
)
{
    /* Mantiene una riga per ciascun sensore anche quando il ciclo fallisce. */
    for (size_t index = 0; index < SENSOR_COUNT; index++) {
        char value[VALUE_BUFFER_SIZE];
        int written = snprintf(value, sizeof(value), "ERRORE: %s", error);
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

/* Interroga tutti gli OID configurati e registra valori o errori. */
static int poll_sensors(
    netsnmp_session *session,
    FILE *csv,
    const char *host,
    unsigned int cycle,
    sensor_name_mode_t name_mode
)
{
    char timestamp[64];
    /* Un unico orario identifica tutti i valori dello stesso ciclo. */
    if (format_timestamp(timestamp, sizeof(timestamp)) != 0) {
        return -1;
    }

    /* La tabella a terminale precede i risultati del ciclo corrente. */
    printf("\n[%s] Ciclo %u - IP: %s\n", timestamp, cycle, host);
    const char *identifier_header = name_mode == SENSOR_NAME_SIMPLE
        ? "SENSORE"
        : (name_mode == SENSOR_NAME_MIB ? "OID MIB" : "SENSORE | OID MIB");
    printf("%-78s %s\n", identifier_header, "VALORE");
    printf("----------------------------------------------------------------------------------------------\n");

    /* La richiesta GET raggruppa tutti gli OID in una singola interrogazione. */
    netsnmp_pdu *request = snmp_pdu_create(SNMP_MSG_GET);
    if (request == NULL) {
        errno = ENOMEM;
        return -1;
    }

    /* Gli OID vengono aggiunti senza valore: il dispositivo li restituisce. */
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
    /* Errori di trasporto o SNMP diventano righe esplicite nel CSV. */
    if (status != STAT_SUCCESS || response == NULL ||
        response->errstat != SNMP_ERR_NOERROR) {
        const char *error = status == STAT_TIMEOUT
            ? "timeout SNMP"
            : (status == STAT_SUCCESS && response != NULL
                ? snmp_errstring(response->errstat)
                : "errore di comunicazione SNMP");
        int result = record_cycle_error(csv, timestamp, error, name_mode);
        if (response != NULL) {
            snmp_free_pdu(response);
        }
        return result;
    }

    /* Net-SNMP restituisce le variabili nello stesso ordine della richiesta. */
    netsnmp_variable_list *variable = response->variables;
    for (size_t index = 0; index < SENSOR_COUNT; index++) {
        char value[VALUE_BUFFER_SIZE];

        if (variable == NULL) {
            snprintf(value, sizeof(value), "OID assente nella risposta SNMP");
        } else {
            int written = snprint_value(
                value,
                sizeof(value),
                variable->name,
                variable->name_length,
                variable
            );
            if (written < 0 || (size_t)written >= sizeof(value)) {
                snprintf(value, sizeof(value), "Valore troppo lungo");
            } else {
                /* Evita che un valore SNMP mandi a capo la riga di output. */
                for (char *character = value; *character != '\0'; character++) {
                    if (*character == '\n' || *character == '\r') {
                        *character = ' ';
                    }
                }
                if (append_sensor_unit(
                        value,
                        sizeof(value),
                        &sensors[index],
                        variable
                    ) != 0) {
                    snprintf(value, sizeof(value), "Valore troppo lungo");
                }
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

    /* La risposta non serve più dopo aver copiato e salvato tutti i valori. */
    snmp_free_pdu(response);
    return 0;
}

/* Confronta due istanti timespec in ordine cronologico. */
static int compare_timespec(
    const struct timespec *left,
    const struct timespec *right
)
{
    /* Confronta prima i secondi e poi i nanosecondi per l'ordine preciso. */
    if (left->tv_sec != right->tv_sec) {
        return left->tv_sec < right->tv_sec ? -1 : 1;
    }
    if (left->tv_nsec != right->tv_nsec) {
        return left->tv_nsec < right->tv_nsec ? -1 : 1;
    }
    return 0;
}

/* Attende con CLOCK_MONOTONIC fino alla scadenza specificata. */
static int sleep_until(const struct timespec *deadline)
{
    int result;
    /* TIMER_ABSTIME attende una scadenza fissa e riprova se arriva un segnale. */
    do {
        result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, deadline, NULL);
    } while (result == EINTR);

    if (result != 0) {
        errno = result;
        return -1;
    }
    return 0;
}

/* Gestisce opzioni, sessione SNMP, raccolta dati e chiusura delle risorse. */
int main(int argc, char **argv)
{
    const char *host = DEFAULT_HOST;
    const char *mib_dir = NULL;
    const char *csv_path = NULL;
    unsigned int duration = DEFAULT_DURATION_SECONDS;
    unsigned int interval = DEFAULT_INTERVAL_SECONDS;
    sensor_name_mode_t name_mode = SENSOR_NAME_SIMPLE;
    int option;

    /* Applica gli eventuali parametri passati dall'utente. */
    while ((option = getopt(argc, argv, "H:M:o:d:i:n:h")) != -1) {
        switch (option) {
        case 'H':
            host = optarg;
            break;
        case 'M':
            mib_dir = optarg;
            break;
        case 'o':
            csv_path = optarg;
            break;
        case 'd':
            if (parse_positive_seconds(optarg, &duration) != 0) {
                fprintf(stderr, "Durata non valida: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'i':
            if (parse_positive_seconds(optarg, &interval) != 0) {
                fprintf(stderr, "Intervallo non valido: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 'n':
            if (parse_sensor_name_mode(optarg, &name_mode) != 0) {
                fprintf(
                    stderr,
                    "Modalita' nomi non valida: %s "
                    "(usa semplice, mib oppure entrambi).\n",
                    optarg
                );
                return EXIT_FAILURE;
            }
            break;
        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* La community non viene salvata nel sorgente: arriva dall'ambiente. */
    const char *community = getenv("ECC_COMMUNITY");
    if (community == NULL || community[0] == '\0') {
        fprintf(stderr, "Imposta la community nella variabile ECC_COMMUNITY.\n");
        return EXIT_FAILURE;
    }

    /* Se mancano i percorsi, usa CSV con data/ora e MIB sotto $HOME/MIB. */
    char default_csv_path[MAX_PATH_LENGTH];
    if (csv_path == NULL) {
        if (make_default_csv_path(default_csv_path,
                                  sizeof(default_csv_path)) != 0) {
            perror("Creazione nome file CSV");
            return EXIT_FAILURE;
        }
        csv_path = default_csv_path;
    }

    char default_mib_dir[MAX_PATH_LENGTH];
    if (mib_dir == NULL) {
        const char *home = getenv("HOME");
        if (home == NULL) {
            fprintf(stderr, "HOME non impostata: specifica la directory con -M.\n");
            return EXIT_FAILURE;
        }
        int written = snprintf(
            default_mib_dir,
            sizeof(default_mib_dir),
            "%s/MIB",
            home
        );
        if (written < 0 || (size_t)written >= sizeof(default_mib_dir)) {
            fprintf(stderr, "Percorso della directory MIB troppo lungo.\n");
            return EXIT_FAILURE;
        }
        mib_dir = default_mib_dir;
    }

    /* Carica i nomi MIB e inizializza la libreria SNMP prima delle richieste. */
    if (snmp_oid_init(mib_dir) != 0) {
        perror("Errore caricamento MIB");
        return EXIT_FAILURE;
    }
    if (netsnmp_ds_set_boolean(
            NETSNMP_DS_LIBRARY_ID,
            NETSNMP_DS_LIB_DONT_PERSIST_STATE,
            1
        ) != SNMPERR_SUCCESS) {
        fprintf(stderr, "Impossibile inizializzare le opzioni Net-SNMP.\n");
        return EXIT_FAILURE;
    }
    init_snmp("ecc-monitor");

    /* Risolve ogni nome simbolico in una sequenza numerica di sottoidentificatori. */
    for (size_t index = 0; index < SENSOR_COUNT; index++) {
        sensors[index].oid_length = MAX_OID_LEN;
        if (snmp_parse_oid(
                sensors[index].oid_text,
                sensors[index].oid_value,
                &sensors[index].oid_length
            ) == NULL) {
            fprintf(stderr, "OID non riconosciuto nel MIB: %s\n",
                    sensors[index].oid_text);
            snmp_shutdown("ecc-monitor");
            return EXIT_FAILURE;
        }
    }

    /* Configura una sessione SNMPv2c con timeout di un secondo e zero retry. */
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
        snmp_perror("Connessione SNMP");
        snmp_shutdown("ecc-monitor");
        return EXIT_FAILURE;
    }

    /* Crea il file e scrive le colonne prima di registrare i campioni. */
    FILE *csv = fopen(csv_path, "w");
    if (csv == NULL) {
        perror(csv_path);
        snmp_close(snmp_session);
        snmp_shutdown("ecc-monitor");
        return EXIT_FAILURE;
    }
    if (fputs("\"timestamp\",\"sensore\",\"oid\",\"valore\"\n", csv) == EOF) {
        perror("Scrittura intestazione CSV");
        fclose(csv);
        snmp_close(snmp_session);
        snmp_shutdown("ecc-monitor");
        return EXIT_FAILURE;
    }

    printf("Raccolta per %u s, intervallo %u s. CSV: %s\n",
           duration, interval, csv_path);

    /* CLOCK_MONOTONIC misura la durata senza risentire dei cambi dell'ora. */
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
    int exit_status = EXIT_SUCCESS;

    /* Raccoglie finché il prossimo ciclo può iniziare prima della scadenza. */
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

        /* Ogni ciclo interroga tutti gli OID e registra risultati o errori. */
        cycle++;
        if (poll_sensors(snmp_session, csv, host, cycle, name_mode) != 0) {
            perror("Errore durante raccolta o salvataggio");
            exit_status = EXIT_FAILURE;
            break;
        }

        /* Calcola la prossima scadenza a partire dalla cadenza prevista. */
        next_poll.tv_sec += interval;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            perror("clock_gettime");
            exit_status = EXIT_FAILURE;
            break;
        }
        /* Se il ciclo è durato troppo, salta scadenze passate senza recuperarle. */
        while (compare_timespec(&next_poll, &now) < 0) {
            next_poll.tv_sec += interval;
        }
        /* Non attende oltre la fine della raccolta. */
        if (compare_timespec(&next_poll, &finish) < 0 &&
            sleep_until(&next_poll) != 0) {
            perror("Attesa tra i campioni");
            exit_status = EXIT_FAILURE;
            break;
        }
    }

    /* Chiude file e sessione anche dopo un errore avvenuto durante il ciclo. */
    if (fclose(csv) != 0) {
        perror("Chiusura CSV");
        exit_status = EXIT_FAILURE;
    }
    snmp_close(snmp_session);
    snmp_shutdown("ecc-monitor");
    printf("\nRaccolta terminata: %u cicli registrati in %s\n",
           cycle, csv_path);
    return exit_status;
}
