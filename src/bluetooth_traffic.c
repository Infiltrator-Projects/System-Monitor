// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file bluetooth_traffic.c
 * @brief Exact per-device Bluetooth accounting from Linux HCI monitor frames.
 *
 * The package grants CAP_NET_RAW only so this process can bind the read-only
 * HCI monitor socket. The capability set is cleared before the reader thread
 * or application monitor lifecycle is started.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "bluetooth_traffic.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#include <errno.h>
#include <linux/capability.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef HCI_CHANNEL_MONITOR
#define HCI_CHANNEL_MONITOR 2
#endif
#ifndef HCI_DEV_NONE
#define HCI_DEV_NONE 0xffff
#endif

#define LSM_HCI_MON_HEADER_SIZE 6U
#define LSM_HCI_MON_ACL_TX 4U
#define LSM_HCI_MON_ACL_RX 5U
#define LSM_HCI_MON_SCO_TX 6U
#define LSM_HCI_MON_SCO_RX 7U
#define LSM_HCI_MON_ISO_TX 18U
#define LSM_HCI_MON_ISO_RX 19U
#define LSM_HCI_HANDLE_MASK 0x0fffU
#define LSM_HCI_ISO_LENGTH_MASK 0x3fffU
#define LSM_HCI_LINK_SLOTS 128U
#define LSM_HCI_MAX_CONNECTIONS 64U
#define LSM_HCI_MONITOR_BUFFER 8192U

typedef struct {
    uint16_t controller_index;
    uint16_t handle;
    char address[32];
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    bool active;
    bool used;
} LsmBluetoothLinkCounter;

typedef struct {
    pthread_mutex_t mutex;
    pthread_t thread;
    int monitor_fd;
    bool thread_started;
    bool stop_requested;
    bool capture_available;
    LsmBluetoothLinkCounter links[LSM_HCI_LINK_SLOTS];
} LsmBluetoothCaptureState;

static LsmBluetoothCaptureState capture_state = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .monitor_fd = -1
};

static uint16_t read_le16(const unsigned char *bytes)
{
    return (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8U);
}

static uint64_t add_saturating(uint64_t left, uint64_t right)
{
    return UINT64_MAX - left < right ? UINT64_MAX : left + right;
}

static bool controller_index(const char *controller, uint16_t *index)
{
    if (!controller || !index || strncmp(controller, "hci", 3U) != 0 ||
        controller[3] == '\0')
        return false;
    errno = 0;
    char *end = NULL;
    const unsigned long value = strtoul(controller + 3U, &end, 10);
    if (errno != 0 || !end || *end != '\0' || value > UINT16_MAX)
        return false;
    *index = (uint16_t)value;
    return true;
}

static void address_string(const bdaddr_t *address, char *buffer, size_t size)
{
    if (!address || !buffer || size == 0U) return;
    (void)snprintf(
        buffer, size, "%02X:%02X:%02X:%02X:%02X:%02X",
        address->b[5], address->b[4], address->b[3],
        address->b[2], address->b[1], address->b[0]);
}

static bool drop_all_capabilities(void)
{
#if defined(SYS_capset)
    struct __user_cap_header_struct header = {
        .version = _LINUX_CAPABILITY_VERSION_3,
        .pid = 0
    };
    struct __user_cap_data_struct data[2];
    memset(data, 0, sizeof(data));
    return syscall(SYS_capset, &header, data) == 0;
#else
    return false;
#endif
}

bool lsm_bluetooth_traffic_parse_monitor(
    const void *buffer, size_t length, LsmBluetoothMonitorPacket *packet)
{
    if (!buffer || !packet || length < LSM_HCI_MON_HEADER_SIZE) return false;
    const unsigned char *bytes = buffer;
    const uint16_t opcode = read_le16(bytes);
    const uint16_t index = read_le16(bytes + 2U);
    const uint16_t payload_length = read_le16(bytes + 4U);
    if ((size_t)payload_length > length - LSM_HCI_MON_HEADER_SIZE)
        return false;

    const unsigned char *payload = bytes + LSM_HCI_MON_HEADER_SIZE;
    size_t header_size = 0U;
    uint64_t data_length = 0U;
    bool receive = false;
    switch (opcode) {
        case LSM_HCI_MON_ACL_TX:
        case LSM_HCI_MON_ACL_RX:
            if (payload_length < 4U) return false;
            header_size = 4U;
            data_length = read_le16(payload + 2U);
            receive = opcode == LSM_HCI_MON_ACL_RX;
            break;
        case LSM_HCI_MON_SCO_TX:
        case LSM_HCI_MON_SCO_RX:
            if (payload_length < 3U) return false;
            header_size = 3U;
            data_length = payload[2U];
            receive = opcode == LSM_HCI_MON_SCO_RX;
            break;
        case LSM_HCI_MON_ISO_TX:
        case LSM_HCI_MON_ISO_RX:
            if (payload_length < 4U) return false;
            header_size = 4U;
            data_length =
                (uint64_t)(read_le16(payload + 2U) & LSM_HCI_ISO_LENGTH_MASK);
            receive = opcode == LSM_HCI_MON_ISO_RX;
            break;
        default:
            return false;
    }
    if (data_length > (uint64_t)((size_t)payload_length - header_size))
        return false;

    packet->controller_index = index;
    packet->handle =
        (uint16_t)(read_le16(payload) & LSM_HCI_HANDLE_MASK);
    packet->payload_bytes = data_length;
    packet->receive = receive;
    return true;
}

static LsmBluetoothLinkCounter *link_for_handle_locked(
    uint16_t controller, uint16_t handle)
{
    LsmBluetoothLinkCounter *free_slot = NULL;
    for (size_t index = 0U; index < LSM_HCI_LINK_SLOTS; index++) {
        LsmBluetoothLinkCounter *slot = &capture_state.links[index];
        if (slot->used && slot->controller_index == controller &&
            slot->handle == handle)
            return slot;
        if (!slot->used && !free_slot) free_slot = slot;
    }
    if (!free_slot) {
        for (size_t index = 0U; index < LSM_HCI_LINK_SLOTS; index++) {
            if (!capture_state.links[index].active) {
                free_slot = &capture_state.links[index];
                break;
            }
        }
    }
    if (!free_slot) return NULL;
    memset(free_slot, 0, sizeof(*free_slot));
    free_slot->used = true;
    free_slot->controller_index = controller;
    free_slot->handle = handle;
    return free_slot;
}

static void account_packet(const LsmBluetoothMonitorPacket *packet)
{
    if (!packet || packet->payload_bytes == 0U) return;
    (void)pthread_mutex_lock(&capture_state.mutex);
    LsmBluetoothLinkCounter *slot = link_for_handle_locked(
        packet->controller_index, packet->handle);
    if (slot) {
        if (packet->receive)
            slot->rx_bytes = add_saturating(
                slot->rx_bytes, packet->payload_bytes);
        else
            slot->tx_bytes = add_saturating(
                slot->tx_bytes, packet->payload_bytes);
    }
    (void)pthread_mutex_unlock(&capture_state.mutex);
}

static void *monitor_reader(void *unused)
{
    (void)unused;
    unsigned char buffer[LSM_HCI_MONITOR_BUFFER];
    for (;;) {
        (void)pthread_mutex_lock(&capture_state.mutex);
        const bool stop = capture_state.stop_requested;
        const int descriptor = capture_state.monitor_fd;
        (void)pthread_mutex_unlock(&capture_state.mutex);
        if (stop || descriptor < 0) break;

        const ssize_t received = recv(descriptor, buffer, sizeof(buffer), 0);
        if (received < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            (void)pthread_mutex_lock(&capture_state.mutex);
            capture_state.capture_available = false;
            (void)pthread_mutex_unlock(&capture_state.mutex);
            break;
        }
        if (received == 0) continue;

        LsmBluetoothMonitorPacket packet;
        if (lsm_bluetooth_traffic_parse_monitor(
                buffer, (size_t)received, &packet))
            account_packet(&packet);
    }
    return NULL;
}

LsmBluetoothTrafficStartResult lsm_bluetooth_traffic_start(void)
{
    (void)pthread_mutex_lock(&capture_state.mutex);
    if (capture_state.thread_started) {
        (void)pthread_mutex_unlock(&capture_state.mutex);
        return LSM_BLUETOOTH_TRAFFIC_STARTED;
    }
    capture_state.stop_requested = false;
    capture_state.capture_available = false;
    memset(capture_state.links, 0, sizeof(capture_state.links));
    (void)pthread_mutex_unlock(&capture_state.mutex);

    int descriptor = socket(
        AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    bool bound = false;
    if (descriptor >= 0) {
        const int receive_buffer = 4 * 1024 * 1024;
        (void)setsockopt(descriptor, SOL_SOCKET, SO_RCVBUF,
                         &receive_buffer, sizeof(receive_buffer));
        const struct timeval timeout = {.tv_sec = 0, .tv_usec = 500000};
        (void)setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO,
                         &timeout, sizeof(timeout));
        struct sockaddr_hci address;
        memset(&address, 0, sizeof(address));
        address.hci_family = AF_BLUETOOTH;
        address.hci_dev = HCI_DEV_NONE;
        address.hci_channel = HCI_CHANNEL_MONITOR;
        bound = bind(descriptor, (struct sockaddr *)&address,
                     sizeof(address)) == 0;
    }

    if (!drop_all_capabilities()) {
        if (descriptor >= 0) (void)close(descriptor);
        return LSM_BLUETOOTH_TRAFFIC_SECURITY_FAILURE;
    }
    if (!bound) {
        if (descriptor >= 0) (void)close(descriptor);
        return LSM_BLUETOOTH_TRAFFIC_UNAVAILABLE;
    }

    (void)pthread_mutex_lock(&capture_state.mutex);
    capture_state.monitor_fd = descriptor;
    capture_state.capture_available = true;
    const int result = pthread_create(
        &capture_state.thread, NULL, monitor_reader, NULL);
    if (result != 0) {
        capture_state.monitor_fd = -1;
        capture_state.capture_available = false;
        (void)pthread_mutex_unlock(&capture_state.mutex);
        (void)close(descriptor);
        return LSM_BLUETOOTH_TRAFFIC_UNAVAILABLE;
    }
    capture_state.thread_started = true;
    (void)pthread_mutex_unlock(&capture_state.mutex);
    return LSM_BLUETOOTH_TRAFFIC_STARTED;
}

void lsm_bluetooth_traffic_stop(void)
{
    (void)pthread_mutex_lock(&capture_state.mutex);
    if (!capture_state.thread_started) {
        const int descriptor = capture_state.monitor_fd;
        capture_state.monitor_fd = -1;
        capture_state.capture_available = false;
        (void)pthread_mutex_unlock(&capture_state.mutex);
        if (descriptor >= 0) (void)close(descriptor);
        return;
    }
    capture_state.stop_requested = true;
    const int descriptor = capture_state.monitor_fd;
    const pthread_t thread = capture_state.thread;
    (void)pthread_mutex_unlock(&capture_state.mutex);

    if (descriptor >= 0) (void)shutdown(descriptor, SHUT_RDWR);
    (void)pthread_join(thread, NULL);
    if (descriptor >= 0) (void)close(descriptor);

    (void)pthread_mutex_lock(&capture_state.mutex);
    capture_state.thread_started = false;
    capture_state.monitor_fd = -1;
    capture_state.capture_available = false;
    capture_state.stop_requested = false;
    memset(capture_state.links, 0, sizeof(capture_state.links));
    (void)pthread_mutex_unlock(&capture_state.mutex);
}

bool lsm_bluetooth_traffic_refresh_connections(const char *controller)
{
    uint16_t index = 0U;
    if (!controller_index(controller, &index)) return false;

    const int descriptor = socket(
        AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    if (descriptor < 0) return false;

    const size_t allocation =
        sizeof(struct hci_conn_list_req) +
        LSM_HCI_MAX_CONNECTIONS * sizeof(struct hci_conn_info);
    struct hci_conn_list_req *list = calloc(1U, allocation);
    if (!list) {
        (void)close(descriptor);
        return false;
    }
    list->dev_id = index;
    list->conn_num = LSM_HCI_MAX_CONNECTIONS;
    const bool okay = ioctl(descriptor, HCIGETCONNLIST, list) == 0;
    (void)close(descriptor);
    if (!okay) {
        free(list);
        return false;
    }

    (void)pthread_mutex_lock(&capture_state.mutex);
    for (size_t slot_index = 0U; slot_index < LSM_HCI_LINK_SLOTS;
         slot_index++) {
        LsmBluetoothLinkCounter *slot = &capture_state.links[slot_index];
        if (slot->used && slot->controller_index == index)
            slot->active = false;
    }

    for (uint16_t connection = 0U; connection < list->conn_num; connection++) {
        const struct hci_conn_info *info = &list->conn_info[connection];
        LsmBluetoothLinkCounter *slot = link_for_handle_locked(
            index, info->handle);
        if (!slot) continue;
        char remote[32] = {0};
        address_string(&info->bdaddr, remote, sizeof(remote));
        if (slot->address[0] &&
            strcasecmp(slot->address, remote) != 0) {
            slot->rx_bytes = 0U;
            slot->tx_bytes = 0U;
        }
        (void)snprintf(slot->address, sizeof(slot->address), "%s", remote);
        slot->active = true;
    }
    (void)pthread_mutex_unlock(&capture_state.mutex);
    free(list);
    return true;
}

bool lsm_bluetooth_traffic_read_device(
    const char *controller, const char *address,
    LsmBluetoothTrafficCounters *counters)
{
    if (!controller || !address || !address[0] || !counters) return false;
    memset(counters, 0, sizeof(*counters));
    uint16_t index = 0U;
    if (!controller_index(controller, &index)) return false;

    (void)pthread_mutex_lock(&capture_state.mutex);
    const bool available = capture_state.capture_available;
    if (available) {
        for (size_t slot_index = 0U; slot_index < LSM_HCI_LINK_SLOTS;
             slot_index++) {
            const LsmBluetoothLinkCounter *slot =
                &capture_state.links[slot_index];
            if (!slot->used || slot->controller_index != index ||
                !slot->address[0] ||
                strcasecmp(slot->address, address) != 0)
                continue;
            counters->rx_bytes = add_saturating(
                counters->rx_bytes, slot->rx_bytes);
            counters->tx_bytes = add_saturating(
                counters->tx_bytes, slot->tx_bytes);
            if (slot->active && counters->link_count < UINT_MAX)
                counters->link_count++;
        }
    }
    (void)pthread_mutex_unlock(&capture_state.mutex);
    return available && counters->link_count > 0U;
}

void lsm_bluetooth_traffic_apply_device(
    LsmBluetoothDeviceInfo *device, LsmBluetoothTrafficState *state,
    const LsmBluetoothTrafficCounters *counters, double elapsed_seconds)
{
    if (!device || !state || !counters) return;
    device->traffic_available = true;
    device->link_count = counters->link_count;
    device->rx_bytes_total = counters->rx_bytes;
    device->tx_bytes_total = counters->tx_bytes;
    device->rx_bytes_per_sec = 0.0;
    device->tx_bytes_per_sec = 0.0;

    if (state->initialized && isfinite(elapsed_seconds) &&
        elapsed_seconds > 0.0) {
        if (counters->rx_bytes >= state->previous_rx)
            device->rx_bytes_per_sec =
                (double)(counters->rx_bytes - state->previous_rx) /
                elapsed_seconds;
        if (counters->tx_bytes >= state->previous_tx)
            device->tx_bytes_per_sec =
                (double)(counters->tx_bytes - state->previous_tx) /
                elapsed_seconds;
    }
    state->previous_rx = counters->rx_bytes;
    state->previous_tx = counters->tx_bytes;
    state->initialized = true;
}

void lsm_bluetooth_traffic_mark_device_unavailable(
    LsmBluetoothDeviceInfo *device, LsmBluetoothTrafficState *state)
{
    if (!device || !state) return;
    device->traffic_available = false;
    device->link_count = 0U;
    device->rx_bytes_per_sec = 0.0;
    device->tx_bytes_per_sec = 0.0;
    state->initialized = false;
}
