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
    "# Vendor rows record authoritative public PCI vendor assignments as factual identifiers.\n"
    "# Device rows are admitted only after independent corroboration from primary/public\n"
    "# hardware evidence. This table is not derived from any third-party PCI names database.\n"
    "V\t025E\tSK hynix-Solidigm\n"
    "V\t1013\tCirrus Logic, Inc.\n"
    "V\t1022\tAdvanced Micro Devices, Inc.\n"
    "V\t1028\tDell Computer Corporation\n"
    "V\t102B\tMatrox Graphics Inc.\n"
    "V\t1043\tAsustek Computer Inc.\n"
    "V\t104C\tTexas Instruments\n"
    "V\t105A\tPromise Technology, Inc.\n"
    "V\t106B\tApple Computer\n"
    "V\t10CF\tFujitsu Limited\n"
    "V\t10DE\tNVidia Corporation\n"
    "V\t10EC\tRealtek Semiconductor Corporation\n"
    "V\t1102\tCreative Technology Ltd\n"
    "V\t1131\tNXP Semiconductors\n"
    "V\t1137\tCisco Systems, Inc.\n"
    "V\t1166\tBroadcom Limited\n"
    "V\t1172\tAltera\n"
    "V\t1180\tRicoh Company, Ltd.\n"
    "V\t11D4\tAnalog Devices International\n"
    "V\t11F8\tMicrochip Technology\n"
    "V\t125B\tASIX Electronics Corp.\n"
    "V\t1283\tITE Tech. Inc.\n"
    "V\t1344\tMicron Technology, Inc.\n"
    "V\t13B5\tARM Ltd.\n"
    "V\t1414\tMicrosoft\n"
    "V\t1425\tChelsio Communications\n"
    "V\t144D\tSamsung Electronics Co., Ltd.\n"
    "V\t1458\tGiga-Byte Technology Co., Ltd.\n"
    "V\t1462\tMicro-Star International Co., Ltd.\n"
    "V\t14C3\tMediaTek Incorporation\n"
    "V\t15B7\tSandisk Technologies, Inc.\n"
    "V\t15D9\tSuper Micro Computer Inc.\n"
    "V\t17AA\tLenovo\n"
    "V\t17CB\tQualcomm Incorporated\n"
    "V\t17CD\tCadence Design Systems\n"
    "V\t17D3\tAreca Technology Corporation\n"
    "V\t1987\tPhison Electronics Corporation\n"
    "V\t1A03\tASPEED Technology Inc.\n"
    "V\t1AE0\tGoogle, Inc.\n"
    "V\t1BAA\tQNAP Systems, Inc.\n"
    "V\t1D0F\tAmazon.com Services LLC\n"
    "V\t1D87\tRockchip Electronics Co., Ltd.\n"
    "V\t1DCA\tMarvell Semiconductor, Inc.\n"
    "V\t1E0F\tKioxia Corporation\n"
    "V\t1E52\tTenstorrent Inc\n"
    "V\t1FE9\tMemryX Inc.\n"
    "V\t20B7\tWestern Digital Technologies, Inc.\n"
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
