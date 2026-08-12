// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file bundled_pci_smoke.c
 * @brief Bundled PCI name resolver regression test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "pci_names.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    char vendor[256] = "";
    char product[256] = "";
    if (!lsm_pci_names_lookup("8086", "7e40",
                              vendor, sizeof(vendor),
                              product, sizeof(product))) {
        fputs("PCI lookup returned no result.\n", stderr);
        return 1;
    }
    if (strcmp(vendor, "Intel Corporation") != 0 ||
        strcmp(product, "Meteor Lake PCH CNVi WiFi") != 0) {
        fprintf(stderr, "Unexpected PCI identity: %s / %s\n", vendor, product);
        return 2;
    }
    printf("%s — %s\n", vendor, product);
    return 0;
}
