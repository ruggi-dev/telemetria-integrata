# Telemetria integrata

Monitor di utilizzo CPU e throughput I/O dei dischi per Linux e WSL 2.

## Requisiti

Il programma legge `/proc/stat`, `/proc/diskstats` e `/sys/dev/block`, quindi
non funziona come applicazione nativa Windows.

Su Ubuntu o Ubuntu in WSL 2 installa compilatore e `make`:

```bash
sudo apt update
sudo apt install build-essential
```

## Compilazione

Dalla cartella del progetto:

```bash
make
```

Il Makefile usa GCC con le opzioni `-std=gnu99 -Wall -Wextra -O2`. Per
ricompilare da zero, prima rimuovi l'eseguibile:

```bash
make clean
make
```

## Avvio

```bash
./telemetria
```

La schermata si aggiorna ogni secondo. Per terminare il programma premi
`Ctrl+C`.

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
