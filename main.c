#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "config.h"

typedef struct {
    unsigned long long user;        // Modalità utente
    unsigned long long nice;        // Priorità modificata
    unsigned long long system;      // Modalità kernel
    unsigned long long idle;        // Inattività
    unsigned long long iowait;      // Attesa I/O
    unsigned long long irq;         // Interruzioni hardware
    unsigned long long softirq;     // Interruzioni software
    unsigned long long steal;       // Attesa hypervisor
} cpu_times_t;

typedef struct {
    unsigned long long read_sectors;
    unsigned long long written_sectors;
    unsigned long long io_ms;
    unsigned int devices;
} io_stats_t;

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
    stats->io_ms = 0;
    stats->devices = 0;

    while (fgets(line, sizeof(line), diskstats) != NULL) {
        unsigned int major;
        unsigned int minor;
        unsigned long long reads_completed;
        unsigned long long reads_merged;
        unsigned long long sectors_read;
        unsigned long long read_ms;
        unsigned long long writes_completed;
        unsigned long long writes_merged;
        unsigned long long sectors_written;
        unsigned long long write_ms;
        unsigned long long io_in_progress;
        unsigned long long io_ms;
        unsigned long long weighted_io_ms;
        char device[128];

        int fields = sscanf(
            line,
            "%u %u %127s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
            &major,
            &minor,
            device,
            &reads_completed,
            &reads_merged,
            &sectors_read,
            &read_ms,
            &writes_completed,
            &writes_merged,
            &sectors_written,
            &write_ms,
            &io_in_progress,
            &io_ms,
            &weighted_io_ms
        );
        if (fields != 14) {
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
        stats->io_ms += io_ms;
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

int main() {
    cpu_times_t previous;
    cpu_times_t current;
    io_stats_t previous_io;
    io_stats_t current_io;

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

        unsigned long long previous_idle = previous.idle + previous.iowait;
        unsigned long long current_idle = current.idle + current.iowait;

        unsigned long long previous_total =
            previous.user + previous.nice + previous.system +
            previous_idle + previous.irq + previous.softirq +
            previous.steal;
        unsigned long long current_total =
            current.user + current.nice + current.system +
            current_idle + current.irq + current.softirq +
            current.steal;

        unsigned long long total_delta = current_total - previous_total;
        unsigned long long idle_delta = current_idle - previous_idle;
        unsigned long long busy_delta = total_delta - idle_delta;

        double cpu_usage = total_delta > 0
            ? (100.0 * busy_delta) / total_delta
            : 0.0;

        double sample_seconds = SAMPLE_INTERVAL_MS / 1000.0;
        double read_mbps =
            current_io.read_sectors >= previous_io.read_sectors
            ? (double)(current_io.read_sectors - previous_io.read_sectors)
                * 512.0 / sample_seconds / 1000000.0
            : 0.0;
        double write_mbps =
            current_io.written_sectors >= previous_io.written_sectors
            ? (double)(current_io.written_sectors - previous_io.written_sectors)
                * 512.0 / sample_seconds / 1000000.0
            : 0.0;
        double io_busy_percent =
            current_io.io_ms >= previous_io.io_ms
            ? 100.0 * (current_io.io_ms - previous_io.io_ms)
                / (SAMPLE_INTERVAL_MS * current_io.devices)
            : 0.0;

        printf(
            "Utilizzo CPU: %.2f%% | I/O lettura: %.2f MB/s | "
            "I/O scrittura: %.2f MB/s | Attivita media dischi: %.2f%%\n",
            cpu_usage,
            read_mbps,
            write_mbps,
            io_busy_percent
        );
        fflush(stdout);

        previous = current;
        previous_io = current_io;
        usleep(SAMPLE_INTERVAL_MS * 1000L);
    }
}

