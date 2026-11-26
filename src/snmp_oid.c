#include "snmp_oid.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

static int mib_initialized;

int snmp_oid_init(const char *mib_dir)
{
    if (mib_dir == NULL || mib_dir[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    DIR *directory = opendir(mib_dir);
    if (directory == NULL) {
        return -1;
    }
    closedir(directory);

    if (mib_initialized) {
        errno = EALREADY;
        return -1;
    }

    if (setenv("MIBS", "ALL", 1) != 0) {
        return -1;
    }

    netsnmp_init_mib();
    if (add_mibdir(mib_dir) < 0) {
        errno = EIO;
        return -1;
    }
    if (read_all_mibs() == NULL) {
        errno = ENOENT;
        return -1;
    }

    if (netsnmp_ds_set_int(
            NETSNMP_DS_LIBRARY_ID,
            NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
            NETSNMP_OID_OUTPUT_MODULE
        ) != SNMPERR_SUCCESS) {
        errno = EIO;
        return -1;
    }

    mib_initialized = 1;
    return 0;
}

int snmp_oid_translate(
    const char *input,
    snmp_oid_format_t output_format,
    char *output,
    size_t output_size
)
{
    if (input == NULL || output == NULL || output_size == 0 ||
        (output_format != SNMP_OID_TO_SYMBOLIC &&
         output_format != SNMP_OID_TO_NUMERIC)) {
        errno = EINVAL;
        return -1;
    }
    if (!mib_initialized) {
        errno = EPERM;
        return -1;
    }

    oid parsed_oid[MAX_OID_LEN];
    size_t parsed_oid_len = MAX_OID_LEN;
    if (snmp_parse_oid(input, parsed_oid, &parsed_oid_len) == NULL) {
        errno = ENOENT;
        return -1;
    }

    if (output_format == SNMP_OID_TO_SYMBOLIC) {
        int written = snprint_objid(
            output,
            output_size,
            parsed_oid,
            parsed_oid_len
        );
        if (written < 0 || (size_t)written >= output_size) {
            output[0] = '\0';
            errno = ENOSPC;
            return -1;
        }
        return 0;
    }

    size_t used = 0;
    for (size_t index = 0; index < parsed_oid_len; index++) {
        int written = snprintf(
            output + used,
            output_size - used,
            ".%lu",
            (unsigned long)parsed_oid[index]
        );
        if (written < 0 || (size_t)written >= output_size - used) {
            output[0] = '\0';
            errno = ENOSPC;
            return -1;
        }
        used += (size_t)written;
    }

    return 0;
}
