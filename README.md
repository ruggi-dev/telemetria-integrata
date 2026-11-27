# Integrated Telemetry

CPU and disk I/O throughput monitors for Linux and WSL 2, plus an SNMP
collector for the Huawei FusionModule800.

## Requirements

The system monitor reads `/proc/stat`, `/proc/diskstats`, and `/sys/dev/block`,
so it does not run as a native Windows application.

On Ubuntu or Ubuntu on WSL 2, install the compiler and `make`:

```bash
sudo apt update
sudo apt install build-essential libsnmp-dev
```

## Build

Build all programs from the project directory:

```bash
make
```

Build only the SNMP collector:

```bash
make ecc-monitor
```

Build only the OID example:

```bash
make oid-example
```

Run the collector and OID translation tests:

```bash
make test
```

The tests do not query the live controller: SNMP timeout handling is checked
with a local UDP socket. OID translation tests require the Huawei MIB files in
`$HOME/MIB`; set `ECC_MIB_DIR` if they are stored elsewhere.

The Makefile uses GCC with `-std=gnu99 -Wall -Wextra -O2`. Executables are
created in `build/`: `system-telemetry`, `oid-example`, and `ecc-monitor`.
To rebuild from scratch:

```bash
make clean
make
```

The [snmp_oid.c](./src/snmp_oid.c) module provides `snmp_oid_init()` to load
MIB files from a directory and `snmp_oid_translate()` to convert numeric OIDs
to symbolic names and vice versa. See [snmp_oid.h](./include/snmp_oid.h) for
the function signatures and supported formats.

The SNMPv2c collector polls FusionModule800 and UPS measurements every second
for 20 seconds, prints each sample, and saves it to
`output/ecc-monitor_YYYY-MM-DD_HH-MM-SS.csv`. It collects temperatures,
humidity, power, load, operating states, and rectifier measurements. The SNMP
community is not stored in the source; set `ECC_COMMUNITY` in the environment
before starting the collector. Available options are `-H` (host), `-M` (MIB
directory), `-o` (CSV file), `-d` (duration in seconds), and `-i` (polling
interval in seconds). The `-n` option selects simple sensor names (default),
MIB OIDs, or both; `-h` displays help. Raw SNMP values are shown alongside
their scaled values and units. The CSV always includes the timestamp, simple
sensor name, OID, and value.

C sources are in `src/`, headers in `include/`, the Bash utility in `scripts/`,
tests in `tests/`, and generated CSV files in `output/`. Built executables are
placed in `build/`.

## Run

```bash
./build/system-telemetry
```

The system monitor waits 500 ms between samples and displays CPU and I/O usage.
I/O throughput is calculated using the nominal 500 ms interval, so processing
time may introduce a small approximation. Press `Ctrl+C` to stop it.

To start the SNMP collector:

```bash
./build/ecc-monitor
```

Choose how sensors are identified in terminal output:

```bash
./build/ecc-monitor -n simple
./build/ecc-monitor -n mib
./build/ecc-monitor -n both
```

## Load testing

To generate CPU or I/O load, install `stress-ng`:

```bash
sudo apt install stress-ng
```

Then run one of these commands in a second terminal:

```bash
stress-ng --cpu 0 --timeout 45s --metrics-brief
stress-ng --hdd 1 --hdd-bytes 512M --temp-path /var/tmp --timeout 45s --metrics-brief
stress-ng --cpu 2 --hdd 2 --hdd-opts direct,sync --temp-path /var/tmp --timeout 60s
stress-ng --cpu 0 --hdd 4 --hdd-opts direct,sync --temp-path /var/tmp --timeout 60s
```

The first two commands test CPU and I/O separately; the last two generate
moderate and high combined load. The I/O tests use temporary files up to
512 MB. On WSL, run the test in a Linux filesystem directory, not under
`/mnt/c`, to better measure the disk traffic seen by the program.

## Cleanup

Remove generated executables:

```bash
make clean
```
