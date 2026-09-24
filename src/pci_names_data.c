// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pci_names_data.c
 * @brief Embedded System Monitor PCI identity registry.
 *
 * The factual table is independently maintained in
 * support/resources/data/pci-names.tsv and compiled into the executable so
 * normal hardware identity lookup requires no external names database.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "pci_names_data.h"

static const char lsm_pci_names_data[] =
    "# System Monitor independently maintained PCI identity registry\n"
    "# Format: V<TAB>vendor-id<TAB>vendor name or D<TAB>vendor-id<TAB>device-id<TAB>device name\n"
    "# Vendor assignments are transcribed from public PCI-SIG member assignments.\n"
    "# Device rows are included only when independently corroborated by primary/public\n"
    "# hardware evidence. This is not derived from the Linux PCI ID Repository.\n"
    "V\t1022\tAdvanced Micro Devices, Inc.\n"
    "V\t1028\tDell Computer Corporation\n"
    "V\t1043\tAsustek Computer Inc.\n"
    "V\t106B\tApple Computer\n"
    "V\t10DE\tNVidia Corporation\n"
    "V\t10EC\tRealtek Semiconductor Corporation\n"
    "V\t1131\tNXP Semiconductors\n"
    "V\t1166\tBroadcom Limited\n"
    "V\t1172\tAltera\n"
    "V\t11D4\tAnalog Devices International\n"
    "V\t1344\tMicron Technology, Inc.\n"
    "V\t13B5\tARM Ltd.\n"
    "V\t1414\tMicrosoft\n"
    "V\t144D\tSamsung Electronics Co., Ltd.\n"
    "V\t14C3\tMediaTek Incorporation\n"
    "V\t17AA\tLenovo\n"
    "V\t17CB\tQualcomm Incorporated\n"
    "V\t1987\tPhison Electronics Corporation\n"
    "V\t1A03\tASPEED Technology Inc.\n"
    "V\t1DCA\tMarvell Semiconductor, Inc.\n"
    "V\t1E0F\tKioxia Corporation\n"
    "V\t8086\tIntel Corporation\n"
    "D\t8086\t7D45\tMeteor Lake-P [Intel Graphics]\n"
    "D\t8086\t7D55\tMeteor Lake-P [Intel Arc Graphics]\n"
    "D\t8086\t7E40\tMeteor Lake PCH CNVi WiFi\n";

const char *const lsm_pci_names_chunks[] = {
    lsm_pci_names_data
};

const size_t lsm_pci_names_chunk_sizes[] = {
    sizeof(lsm_pci_names_data) - 1U
};

const size_t lsm_pci_names_chunk_count =
    sizeof(lsm_pci_names_chunks) / sizeof(lsm_pci_names_chunks[0]);
