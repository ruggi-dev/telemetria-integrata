#ifndef SNMP_OID_H
#define SNMP_OID_H

#include <stddef.h>

typedef enum {
    SNMP_OID_TO_SYMBOLIC,
    SNMP_OID_TO_NUMERIC
} snmp_oid_format_t;

/* Load the MIB files from mib_dir. Call once before snmp_oid_translate(). */
int snmp_oid_init(const char *mib_dir);

/*
 * Translate an OID between numeric form and MIB symbolic form.
 * Returns 0 on success, -1 on failure and sets errno.
 */
int snmp_oid_translate(
    const char *input,
    snmp_oid_format_t output_format,
    char *output,
    size_t output_size
);

#endif
