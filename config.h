#ifndef CONFIG_H
#define CONFIG_H

/* Intervallo di campionamento del ciclo principale (500 ms = 2 Hz) */
#define SAMPLE_INTERVAL_MS      500L

/* Soglie per la classificazione delle metriche mostrate */
#define CPU_LOAD_LOW_MAX_PERCENT        30.0
#define CPU_LOAD_MEDIUM_MAX_PERCENT     70.0
#define IO_RATE_LOW_MAX_MBPS            5.0
#define IO_RATE_MEDIUM_MAX_MBPS         50.0

#endif /* CONFIG_H */