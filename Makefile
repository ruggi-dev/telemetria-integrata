CC = gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -O2
TARGET = telemetria

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ main.c $(LDLIBS)

clean:
	rm -f $(TARGET)
