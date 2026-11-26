#include "snmp_oid.h"

#include <stdio.h>

int main(void)
{
    const char *mib_dir = "/home/francesco/MIB";
    const char *oid_numerico =
        ".1.3.6.1.4.1.2011.6.178.1.1.25.1.1.4.4123";
    const char *oid_simbolico =
        "HUAWEI-ECC-MIB::hwTempHumTempSensor.4123";
    char risultato[256];

    /* Carica i MIB prima di effettuare conversioni. */
    if (snmp_oid_init(mib_dir) != 0) {
        perror("snmp_oid_init");
        return 1;
    }

    /* OID numerico -> nome definito nel MIB. */
    if (snmp_oid_translate(oid_numerico, SNMP_OID_TO_SYMBOLIC,
                           risultato, sizeof(risultato)) != 0) {
        perror("conversione a simbolico");
        return 1;
    }
    printf("Simbolico: %s\n", risultato);

    /* Nome definito nel MIB -> OID numerico. */
    if (snmp_oid_translate(oid_simbolico, SNMP_OID_TO_NUMERIC,
                           risultato, sizeof(risultato)) != 0) {
        perror("conversione a numerico");
        return 1;
    }
    printf("Numerico:  %s\n", risultato);

    return 0;
}