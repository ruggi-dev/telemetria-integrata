# telemetria-integrata

COMANDI UTILI PER STRESS-TEST
Carico medio (2 core CPU + scrittura reale su disco per 60 secondi):
  stress-ng --cpu 2 --hdd 2 --hdd-opts direct,sync --temp-path /var/tmp --timeout 60s
Carico massimo 
  stress-ng --cpu 0 --hdd 4 --hdd-opts direct,sync --temp-path /var/tmp --timeout 60s
