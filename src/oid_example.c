#include "snmp_oid.h"

#include <stdio.h>

int main(void)
{
    const char *mib_dir = "/home/francesco/MIB";
    const char *numeric_oid =
        ".1.3.6.1.4.1.2011.6.178.1.1.25.1.1.4.4123";
    const char *symbolic_oid =
        "HUAWEI-ECC-MIB::hwTempHumTempSensor.4123";
    char result[256];

    /* Load MIB files before translating OIDs. */
    if (snmp_oid_init(mib_dir) != 0) {
        perror("snmp_oid_init");
        return 1;
    }

    /* Numeric OID -> MIB symbolic name. */
    if (snmp_oid_translate(
            numeric_oid,
            SNMP_OID_TO_SYMBOLIC,
            result,
            sizeof(result)
        ) != 0) {
        perror("Translate OID to symbolic name");
        return 1;
    }
    printf("Symbolic: %s\n", result);

    /* MIB symbolic name -> numeric OID. */
    if (snmp_oid_translate(
            symbolic_oid,
            SNMP_OID_TO_NUMERIC,
            result,
            sizeof(result)
        ) != 0) {
        perror("Translate OID to numeric form");
        return 1;
    }
    printf("Numeric:   %s\n", result);

    return 0;
}