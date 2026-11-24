#ifndef CONFIG_H
#define CONFIG_H

/* Percorsi IPC e Log */
#define IPC_SOCKET_PATH         "/tmp/itot_telemetry.sock"
#define CGROUP_BASE_PATH        "/sys/fs/cgroup"
#define RAPL_ENERGY_PATH        "/sys/class/powercap/intel-rapl:0/energy_uj"

/* Temporizzazione ciclo principale (500 ms = 2 Hz) */
#define SAMPLE_INTERVAL_MS      1000L
#define NS_PER_MS               1000000L
#define NS_PER_SEC              1000000000L

/* Soglie per la classificazione sintetica della telemetria */
#define CPU_LOAD_LOW_MAX_PERCENT        30.0
#define CPU_LOAD_MEDIUM_MAX_PERCENT     70.0
#define IO_RATE_LOW_MAX_MBPS            5.0
#define IO_RATE_MEDIUM_MAX_MBPS         50.0

/* Soglie termiche FusionModule800 (corridoio caldo, gradi Celsius) con isteresi */
#define TEMP_WARN_ENTER_C       32.0f
#define TEMP_WARN_EXIT_C        30.0f
#define TEMP_CRIT_ENTER_C       36.0f
#define TEMP_CRIT_EXIT_C        33.5f

/* Limiti cgroups v2 (cpu.max: quota in us su periodo di 100000 us) */
#define CGROUP_PERIOD_US        100000
#define CGROUP_QUOTA_NORMAL     "max"       /* 100% CPU disponibile */
#define CGROUP_QUOTA_WARNING    "70000"     /* Cap al 70% */
#define CGROUP_QUOTA_CRITICAL   "30000"     /* Cap al 30% */

/* Stati operativi del sistema (FSM) */
typedef enum {
    STATE_NORMAL = 0,
    STATE_WARNING = 1,
    STATE_CRITICAL = 2
} system_state_t;

#endif /* CONFIG_H */