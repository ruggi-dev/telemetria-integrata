CC = gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -O2
CPPFLAGS += -Iinclude
BUILD_DIR = build
TARGET = $(BUILD_DIR)/system-telemetry
EXAMPLE_TARGET = $(BUILD_DIR)/oid-example
MONITOR_TARGET = $(BUILD_DIR)/ecc-monitor
TEST_TARGET = $(BUILD_DIR)/test-ecc-monitor
NETSNMP_CFLAGS = $(shell net-snmp-config --cflags)
NETSNMP_LIBS = $(shell net-snmp-config --libs)

.PHONY: all clean test oid-example ecc-monitor

all: $(TARGET) $(EXAMPLE_TARGET) $(MONITOR_TARGET)

oid-example: $(EXAMPLE_TARGET)

ecc-monitor: $(MONITOR_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(BUILD_DIR):
	mkdir -p $@

$(TARGET): src/main.c include/config.h src/snmp_oid.c include/snmp_oid.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NETSNMP_CFLAGS) $(LDFLAGS) \
		-o $@ src/main.c src/snmp_oid.c $(LDLIBS) $(NETSNMP_LIBS)

$(EXAMPLE_TARGET): src/oid_example.c src/snmp_oid.c include/snmp_oid.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NETSNMP_CFLAGS) $(LDFLAGS) \
		-o $@ src/oid_example.c src/snmp_oid.c $(LDLIBS) $(NETSNMP_LIBS)

$(MONITOR_TARGET): src/ecc_monitor.c src/snmp_oid.c src/temperature_alarm.c include/snmp_oid.h include/ups_power_mode.h include/temperature_alarm.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NETSNMP_CFLAGS) $(LDFLAGS) \
		-o $@ src/ecc_monitor.c src/snmp_oid.c src/temperature_alarm.c $(LDLIBS) $(NETSNMP_LIBS)

$(TEST_TARGET): tests/test_ecc_monitor.c src/ecc_monitor.c src/snmp_oid.c src/temperature_alarm.c include/snmp_oid.h include/ups_power_mode.h include/temperature_alarm.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NETSNMP_CFLAGS) $(LDFLAGS) \
		-o $@ tests/test_ecc_monitor.c src/snmp_oid.c src/temperature_alarm.c $(LDLIBS) $(NETSNMP_LIBS)

clean:
	rm -f $(TARGET) $(EXAMPLE_TARGET) $(MONITOR_TARGET) $(TEST_TARGET)
