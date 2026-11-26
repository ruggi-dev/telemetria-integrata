# Telemetria integrata

Monitor di utilizzo CPU e throughput I/O dei dischi per Linux e WSL 2.

## Requisiti

Il programma legge `/proc/stat`, `/proc/diskstats` e `/sys/dev/block`, quindi
non funziona come applicazione nativa Windows.

Su Ubuntu o Ubuntu in WSL 2 installa compilatore e `make`:

```bash
sudo apt update
sudo apt install build-essential libsnmp-dev
```

## Compilazione

Dalla cartella del progetto compila i programmi:

```bash
make
```

Per compilare soltanto il raccoglitore SNMP:

```bash
make ecc-monitor
```

Per eseguire i test del raccoglitore e del modulo di traduzione OID:

```bash
make test
```

I test non interrogano la centralina reale: il timeout SNMP viene verificato
con un socket UDP locale. Per i test di traduzione serve il MIB Huawei in
`$HOME/MIB`; se si trova altrove, impostare `ECC_MIB_DIR`.

Il Makefile usa GCC con le opzioni `-std=gnu99 -Wall -Wextra -O2`. I file
eseguibili vengono creati nella cartella `build/`: `telemetria`,
`esempio_oid` ed `ecc-monitor`. Per ricompilare da zero:

```bash
make clean
make
```

Gli eseguibili sono `build/telemetria` e `build/esempio_oid`.

Il modulo [snmp_oid.c](./src/snmp_oid.c) fornisce `snmp_oid_init()` per caricare
i MIB da una directory e `snmp_oid_translate()` per convertire un OID numerico
in nome simbolico o viceversa. Consultare [snmp_oid.h](./include/snmp_oid.h) per la
firma e i formati selezionabili.

Il raccoglitore interroga via SNMPv2c temperatura, umidità e potenza ogni
secondo per 20 secondi, stampa i campioni durante la lettura e li salva in
`output/ecc-monitor_YYYY-MM-DD_HH-MM-SS.csv`. La directory `tests/` è riservata
ai test. La community non è inclusa nel sorgente: impostare
`ECC_COMMUNITY` nell'ambiente prima dell'avvio. Sono disponibili le opzioni
`-H` (host), `-M` (directory MIB), `-o` (file CSV), `-d` (durata in secondi)
e `-i` (intervallo in secondi). L'opzione `-n` sceglie se mostrare il nome
semplice del sensore (predefinito), l'OID MIB o entrambi; `-h` mostra l'aiuto.
I valori SNMP grezzi restano interi e sono mostrati insieme alla conversione
con unità: temperature in °C, umidità relativa in %RH, potenza UPS in kW,
rapporto di carico in percentuale e potenza DC in W.
Il CSV conserva sempre sia il nome semplice sia l'OID, oltre a timestamp e
valore.

I sorgenti C sono in `src/`, gli header in `include/`, lo script Bash originale
in `scripts/`, i test in `tests/` e i CSV generati in `output/`. Gli eseguibili
compilati vengono collocati in `build/`.

## Avvio

```bash
./build/telemetria
```

Il programma attende 500 ms tra un campione e il successivo e mostra i livelli
CPU e I/O con i relativi valori. Il throughput I/O usa l'intervallo nominale
di 500 ms, quindi il tempo di elaborazione può introdurre una lieve
approssimazione. Per terminare premi `Ctrl+C`.

Per avviare il raccoglitore SNMP:

```bash
./build/ecc-monitor
```

Per scegliere come identificare i sensori nell'output del terminale:

```bash
./build/ecc-monitor -n semplice
./build/ecc-monitor -n mib
./build/ecc-monitor -n entrambi
```

## Test del carico

Per provare la CPU o l'I/O, installa `stress-ng`:

```bash
sudo apt install stress-ng
```

Poi esegui uno dei comandi seguenti in un secondo terminale:

```bash
stress-ng --cpu 0 --timeout 45s --metrics-brief
stress-ng --hdd 1 --hdd-bytes 512M --temp-path /var/tmp --timeout 45s --metrics-brief
stress-ng --cpu 2 --hdd 2 --hdd-opts direct,sync --temp-path /var/tmp --timeout 60s
stress-ng --cpu 0 --hdd 4 --hdd-opts direct,sync --temp-path /var/tmp --timeout 60s
```

I primi due comandi provano separatamente CPU e I/O; gli ultimi due generano
un carico combinato moderato e massimo. Il test I/O usa file temporanei fino a
512 MB. In WSL, esegui il test su una directory del filesystem Linux, non su
un percorso `/mnt/c`, per misurare meglio il traffico del disco visto dal
programma.

## Pulizia

Per eliminare l'eseguibile generato:

```bash
make clean
```
