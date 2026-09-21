// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux_hci_abi.h
 * @brief Minimal project-owned declarations for the Linux Bluetooth HCI ABI.
 *
 * System Monitor uses only a tiny kernel-facing subset of the much larger
 * BlueZ development headers. These declarations mirror the stable Linux HCI
 * socket/address and connection-list ioctl layout that the collector consumes;
 * no BlueZ implementation code is required for packet capture.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef SYSTEM_MONITOR_LINUX_HCI_ABI_H
#define SYSTEM_MONITOR_LINUX_HCI_ABI_H

#include <stddef.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#ifndef AF_BLUETOOTH
#define AF_BLUETOOTH 31
#endif

#define LSM_BTPROTO_HCI 1
#define LSM_HCI_DEV_NONE UINT16_C(0xffff)
#define LSM_HCI_CHANNEL_MONITOR UINT16_C(2)
#define LSM_HCIGETCONNLIST _IOR('H', 212, int)

typedef struct {
    uint8_t bytes[6];
} LsmBluetoothAddress;

typedef struct {
    sa_family_t family;
    unsigned short device;
    unsigned short channel;
} LsmSockaddrHci;

typedef struct {
    uint16_t handle;
    LsmBluetoothAddress address;
    uint8_t type;
    uint8_t outgoing;
    uint16_t state;
    uint32_t link_mode;
} LsmHciConnectionInfo;

typedef struct {
    uint16_t device_id;
    uint16_t connection_count;
    LsmHciConnectionInfo connections[];
} LsmHciConnectionList;

_Static_assert(sizeof(LsmBluetoothAddress) == 6U,
               "Linux Bluetooth address ABI changed");
_Static_assert(sizeof(LsmSockaddrHci) == 6U,
               "Linux HCI sockaddr ABI changed");
_Static_assert(offsetof(LsmSockaddrHci, device) == 2U &&
               offsetof(LsmSockaddrHci, channel) == 4U,
               "Linux HCI sockaddr offsets changed");
_Static_assert(sizeof(LsmHciConnectionInfo) == 16U,
               "Linux HCI connection ABI changed");
_Static_assert(offsetof(LsmHciConnectionInfo, address) == 2U &&
               offsetof(LsmHciConnectionInfo, state) == 10U &&
               offsetof(LsmHciConnectionInfo, link_mode) == 12U,
               "Linux HCI connection offsets changed");
_Static_assert(sizeof(LsmHciConnectionList) == 4U &&
               offsetof(LsmHciConnectionList, connections) == 4U,
               "Linux HCI connection-list ABI changed");

#endif
