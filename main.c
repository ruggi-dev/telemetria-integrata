#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "config.h"

typedef struct {
    unsigned long long user;        // Modalita' utente
    unsigned long long nice;        // Priorita' modificata
    unsigned long long system;      // Modalita' kernel
    unsigned long long idle;        // Inattivita'
    unsigned long long iowait;      // Attesa I/O
    unsigned long long irq;         // Interruzioni hardware
    unsigned long long softirq;     // Interruzioni software
    unsigned long long steal;       // Attesa hypervisor
} cpu_times_t;

typedef struct {
    unsigned long long read_sectors;
    unsigned long long written_sectors;
    unsigned int devices;
} io_stats_t;

static unsigned long long cpu_total_time(const cpu_times_t *times)
{
    return times->user + times->nice + times->system +
        times->idle + times->iowait + times->irq +
        times->softirq + times->steal;
}

static double cpu_usage_percent(
    const cpu_times_t *previous,
    const cpu_times_t *current
)
{
    unsigned long long previous_idle = previous->idle + previous->iowait;
    unsigned long long current_idle = current->idle + current->iowait;
    unsigned long long total_delta =
        cpu_total_time(current) - cpu_total_time(previous);
    unsigned long long idle_delta = current_idle - previous_idle;

    return total_delta > 0
        ? 100.0 * (total_delta - idle_delta) / total_delta
        : 0.0;
}

static unsigned long long counter_delta(
    unsigned long long previous,
    unsigned long long current
)
{
    return current >= previous ? current - previous : 0;
}

static double io_rate_mbps(
    const io_stats_t *previous,
    const io_stats_t *current
)
{
    double sample_seconds = SAMPLE_INTERVAL_MS / 1000.0;
    double transferred_sectors =
        (double)counter_delta(previous->read_sectors, current->read_sectors) +
        (double)counter_delta(
            previous->written_sectors,
            current->written_sectors
        );

    return transferred_sectors * 512.0 / sample_seconds / 1000000.0;
}

static const char *classify_level(
    double value,
    double low_max,
    double medium_max
)
{
    if (value < low_max) {
        return "BASSO";
    }
    if (value < medium_max) {
        return "MEDIO";
    }
    return "ALTO";
}

static void print_telemetry(
    double cpu_usage,
    double io_throughput,
    unsigned int refresh
)
{
    static const char spinner[] = {'|', '/', '-', '\\'};
    const char *cpu_level = classify_level(
        cpu_usage,
        CPU_LOAD_LOW_MAX_PERCENT,
        CPU_LOAD_MEDIUM_MAX_PERCENT
    );
    const char *io_level = classify_level(
        io_throughput,
        IO_RATE_LOW_MAX_MBPS,
        IO_RATE_MEDIUM_MAX_MBPS
    );

    printf(
        "\033[2J\033[H"
        "TELEMETRIA SISTEMA\n"
        "Aggiornamento in corso %c\n\n"
        "Utilizzo CPU: %s (%.2f%%)\n"
        "Utilizzo I/O: %s (%.2f MB/s)\n",
        spinner[refresh % (sizeof(spinner) / sizeof(spinner[0]))],
        cpu_level,
        cpu_usage,
        io_level,
        io_throughput
    );
    fflush(stdout);
}

static int read_cpu_times(cpu_times_t *times)
{
    FILE *stat = fopen("/proc/stat", "r");
    char line[256];

    if (stat == NULL) {
        perror("Errore apertura /proc/stat");
        return -1;
    }

    if (fgets(line, sizeof(line), stat) == NULL) {
        fclose(stat);
        fprintf(stderr, "Errore lettura /proc/stat\n");
        return -1;
    }

    fclose(stat);

    unsigned long long guest;
    unsigned long long guest_nice;
    int fields = sscanf(
        line,
        "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        &times->user,
        &times->nice,
        &times->system,
        &times->idle,
        &times->iowait,
        &times->irq,
        &times->softirq,
        &times->steal,
        &guest,
        &guest_nice
    );

    return fields >= 8 ? 0 : -1;
}

static int read_io_stats(io_stats_t *stats)
{
    FILE *diskstats = fopen("/proc/diskstats", "r");
    char line[512];

    if (diskstats == NULL) {
        perror("Errore apertura /proc/diskstats");
        return -1;
    }

    if (access("/sys/dev/block", F_OK) != 0) {
        perror("Errore accesso a /sys/dev/block");
        fclose(diskstats);
        return -1;
    }

    stats->read_sectors = 0;
    stats->written_sectors = 0;
    stats->devices = 0;

    while (fgets(line, sizeof(line), diskstats) != NULL) {
        unsigned int major;
        unsigned int minor;
        unsigned long long reads_completed;
        unsigned long long reads_merged;
        unsigned long long sectors_read;
        unsigned long long read_time_ms;
        unsigned long long writes_completed;
        unsigned long long writes_merged;
        unsigned long long sectors_written;
        char device[128];

        /* I campi intermedi servono per raggiungere i contatori dei settori. */
        int fields = sscanf(
            line,
            "%u %u %127s %llu %llu %llu %llu %llu %llu %llu",
            &major,
            &minor,
            device,
            &reads_completed,
            &reads_merged,
            &sectors_read,
            &read_time_ms,
            &writes_completed,
            &writes_merged,
            &sectors_written
        );
        if (fields != 10) {
            fprintf(stderr, "Formato non valido in /proc/diskstats\n");
            fclose(diskstats);
            return -1;
        }

        char partition_path[256];
        snprintf(
            partition_path,
            sizeof(partition_path),
            "/sys/dev/block/%u:%u/partition",
            major,
            minor
        );

        errno = 0;
        if (access(partition_path, F_OK) == 0) {
            continue;
        }
        if (errno != ENOENT) {
            perror("Errore verifica partizione in sysfs");
            fclose(diskstats);
            return -1;
        }

        stats->read_sectors += sectors_read;
        stats->written_sectors += sectors_written;
        stats->devices++;
    }

    if (ferror(diskstats)) {
        perror("Errore lettura /proc/diskstats");
        fclose(diskstats);
        return -1;
    }

    fclose(diskstats);
    return stats->devices > 0 ? 0 : -1;
}

int main(void)
{
    cpu_times_t previous;
    cpu_times_t current;
    io_stats_t previous_io;
    io_stats_t current_io;
    unsigned int refresh = 0;

    if (read_cpu_times(&previous) != 0) {
        return EXIT_FAILURE;
    }
    if (read_io_stats(&previous_io) != 0) {
        fprintf(stderr, "Impossibile leggere le statistiche I/O\n");
        return EXIT_FAILURE;
    }

    usleep(SAMPLE_INTERVAL_MS * 1000L);

    while (1) {
        if (read_cpu_times(&current) != 0) {
            fprintf(stderr, "Errore lettura statistiche CPU\n");
            return EXIT_FAILURE;
        }
        if (read_io_stats(&current_io) != 0) {
            fprintf(stderr, "Impossibile leggere le statistiche I/O\n");
            return EXIT_FAILURE;
        }

        double cpu_usage = cpu_usage_percent(&previous, &current);
        double io_throughput = io_rate_mbps(&previous_io, &current_io);

        print_telemetry(cpu_usage, io_throughput, refresh);
        refresh++;

        previous = current;
        previous_io = current_io;
        usleep(SAMPLE_INTERVAL_MS * 1000L);
    }
}

