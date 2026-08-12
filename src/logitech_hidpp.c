// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file logitech_hidpp.c
 * @brief Native Logitech HID++ battery monitoring over Linux hidraw.
 *
 * Only the HID++ 2.0 Root, Battery Status and Unified Battery calls needed for
 * passive monitoring are implemented. Slow wireless transactions run on a
 * dedicated worker, so a sleeping device cannot block GTK sampling.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "logitech_hidpp.h"

#include "common.h"
#include "logitech_hidpp_protocol.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define LSM_HIDPP_REPORT_ID_SHORT 0x10U
#define LSM_HIDPP_REPORT_ID_LONG 0x11U
#define LSM_HIDPP_REPORT_SIZE_SHORT 7U
#define LSM_HIDPP_REPORT_SIZE_LONG 20U
#define LSM_HIDPP_DEVICE_DIRECT 0xffU
#define LSM_HIDPP_ERROR_FEATURE 0xffU
#define LSM_HIDPP_FEATURE_BATTERY_STATUS 0x1000U
#define LSM_HIDPP_FEATURE_UNIFIED_BATTERY 0x1004U
#define LSM_HIDPP_FUNCTION_GET_STATUS 0x00U
#define LSM_HIDPP_FUNCTION_UNIFIED_STATUS 0x10U
#define LSM_HIDPP_IO_TIMEOUT_MS 4000
#define LSM_HIDPP_REFRESH_SECONDS 10.0
#define LSM_HIDPP_RETRY_SECONDS 30.0
#define LSM_HIDPP_PATH_SIZE 512U

typedef struct {
    char device_path[LSM_HIDPP_PATH_SIZE];
    uint16_t feature_id;
    uint8_t feature_index;
    LsmHidppBatteryReading reading;
    double next_query_monotonic;
    bool valid;
} LsmHidppDeviceSlot;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t thread;
    int cancel_pipe[2];
    LsmHidppDeviceSlot devices[LSM_LOGITECH_HIDPP_MAX_DEVICES];
    size_t count;
    bool stop_requested;
    bool thread_started;
} LsmHidppWorkerState;

static LsmHidppWorkerState hidpp_state = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .condition = PTHREAD_COND_INITIALIZER,
    .cancel_pipe = {-1, -1}
};

static const char *hidraw_sys_root(void)
{
    const char *root = getenv("LSM_HIDRAW_SYS_ROOT");
    return root && root[0] ? root : "/sys/class/hidraw";
}

static const char *hidraw_dev_root(void)
{
    const char *root = getenv("LSM_HIDRAW_DEV_ROOT");
    return root && root[0] ? root : "/dev";
}

bool lsm_logitech_hidpp_find_device(const char *power_supply_path,
                                    char *device_path, size_t device_path_size)
{
    if (!power_supply_path || !device_path || device_path_size == 0U)
        return false;
    device_path[0] = '\0';

    char resolved_supply[LSM_HIDPP_PATH_SIZE];
    if (!lsm_realpath_copy(power_supply_path, resolved_supply, sizeof(resolved_supply))) return false;
    char *power_marker = strstr(resolved_supply, "/power_supply/");
    if (!power_marker) return false;
    *power_marker = '\0';

    const char *sys_root = hidraw_sys_root();
    DIR *directory = opendir(sys_root);
    if (!directory) return false;

    bool found = false;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strncmp(entry->d_name, "hidraw", 6U) != 0) continue;
        char link_path[LSM_HIDPP_PATH_SIZE];
        char resolved_device[LSM_HIDPP_PATH_SIZE];
        if (snprintf(link_path, sizeof(link_path), "%s/%s/device", sys_root,
                     entry->d_name) >= (int)sizeof(link_path))
            continue;
        if (!lsm_realpath_copy(link_path, resolved_device, sizeof(resolved_device))) continue;
        if (strcmp(resolved_supply, resolved_device) != 0) continue;
        if (snprintf(device_path, device_path_size, "%s/%s",
                     hidraw_dev_root(), entry->d_name) >=
            (int)device_path_size)
            device_path[0] = '\0';
        found = device_path[0] != '\0';
        break;
    }
    closedir(directory);
    return found;
}

static double monotonic_seconds(void)
{
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec / 1000000000.0;
}

static int remaining_timeout_ms(const struct timespec *deadline)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return LSM_HIDPP_IO_TIMEOUT_MS;
    int64_t remaining_ns =
        ((int64_t)deadline->tv_sec - (int64_t)now.tv_sec) * 1000000000LL +
        ((int64_t)deadline->tv_nsec - (int64_t)now.tv_nsec);
    if (remaining_ns <= 0) return 0;
    const int64_t remaining_ms = (remaining_ns + 999999LL) / 1000000LL;
    return remaining_ms > INT32_MAX ? INT32_MAX : (int)remaining_ms;
}

static int wait_for_descriptor(int descriptor, short events, int cancel_fd,
                               int timeout_ms)
{
    struct pollfd descriptors[2] = {
        {.fd = descriptor, .events = events, .revents = 0},
        {.fd = cancel_fd, .events = POLLIN, .revents = 0}
    };
    const nfds_t count = cancel_fd >= 0 ? 2U : 1U;
    int result;
    do {
        result = poll(descriptors, count, timeout_ms);
    } while (result < 0 && errno == EINTR);
    if (result <= 0) return result;
    if (count == 2U && descriptors[1].revents != 0) return -1;
    if (descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL)) return -1;
    return (descriptors[0].revents & events) != 0 ? 1 : 0;
}

static bool write_report(int descriptor, const uint8_t *report, size_t size,
                         int cancel_fd, const struct timespec *deadline)
{
    for (;;) {
        const ssize_t written = write(descriptor, report, size);
        if (written == (ssize_t)size) return true;
        if (written >= 0) return false;
        if (errno == EINTR) continue;
        if (errno != EAGAIN && errno != EWOULDBLOCK) return false;
        const int timeout = remaining_timeout_ms(deadline);
        if (timeout <= 0 ||
            wait_for_descriptor(descriptor, POLLOUT, cancel_fd, timeout) <= 0)
            return false;
    }
}

static void drain_descriptor(int descriptor)
{
    uint8_t discard[64];
    for (;;) {
        const ssize_t length = read(descriptor, discard, sizeof(discard));
        if (length > 0) continue;
        if (length < 0 && errno == EINTR) continue;
        break;
    }
}

static bool hidpp_request(int descriptor, uint8_t feature_index,
                          uint8_t function, const uint8_t *parameters,
                          size_t parameter_count, uint8_t *payload,
                          size_t payload_size, size_t *received_size,
                          int cancel_fd)
{
    if (received_size) *received_size = 0U;

    uint8_t request[LSM_HIDPP_REPORT_SIZE_LONG] = {0};
    const size_t request_size = lsm_logitech_hidpp_format_request(
        feature_index, function, parameters, parameter_count, request,
        sizeof(request));
    if (request_size == 0U) return false;

    drain_descriptor(descriptor);

    struct timespec deadline;
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) return false;
    deadline.tv_sec += LSM_HIDPP_IO_TIMEOUT_MS / 1000;
    deadline.tv_nsec +=
        (long)(LSM_HIDPP_IO_TIMEOUT_MS % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    if (!write_report(descriptor, request, request_size, cancel_fd,
                      &deadline))
        return false;

    for (;;) {
        const int timeout = remaining_timeout_ms(&deadline);
        if (timeout <= 0 ||
            wait_for_descriptor(descriptor, POLLIN, cancel_fd, timeout) <= 0)
            return false;

        uint8_t response[64];
        ssize_t length;
        do {
            length = read(descriptor, response, sizeof(response));
        } while (length < 0 && errno == EINTR);
        if (length < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (length < 4) continue;
        if (response[0] != LSM_HIDPP_REPORT_ID_SHORT &&
            response[0] != LSM_HIDPP_REPORT_ID_LONG)
            continue;
        if (response[1] != LSM_HIDPP_DEVICE_DIRECT && response[1] != 0x00U)
            continue;

        /* HID++ multiplexes replies from several clients on one hidraw node.
         * Match both the feature index and our function/software-ID byte;
         * error replies encode those original bytes after feature 0xff. */
        if (response[2] == LSM_HIDPP_ERROR_FEATURE &&
            response[3] == feature_index && length >= 6 &&
            response[4] == request[3])
            return false;
        if (response[2] != feature_index || response[3] != request[3])
            continue;

        size_t available = (size_t)length - 4U;
        if (available > payload_size) available = payload_size;
        if (payload && available > 0U) memcpy(payload, &response[4], available);
        if (received_size) *received_size = available;
        return true;
    }
}

static bool root_feature_index(int descriptor, uint16_t feature,
                               uint8_t *feature_index, int cancel_fd)
{
    const uint8_t request[2] = {
        (uint8_t)(feature >> 8U),
        (uint8_t)(feature & 0xffU)
    };
    uint8_t response[16] = {0};
    size_t response_size = 0U;
    if (!hidpp_request(descriptor, 0x00U, 0x00U, request, sizeof(request),
                       response, sizeof(response), &response_size, cancel_fd) ||
        response_size < 1U || response[0] == 0U)
        return false;
    *feature_index = response[0];
    return true;
}

static bool query_device(const char *device_path, uint16_t *feature_id,
                         uint8_t *feature_index,
                         LsmHidppBatteryReading *reading, int cancel_fd)
{
    if (!device_path || !feature_id || !feature_index || !reading)
        return false;
    lsm_logitech_hidpp_reset_reading(reading);

    const int descriptor = open(device_path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (descriptor < 0) return false;

    struct hidraw_devinfo information;
    memset(&information, 0, sizeof(information));
    if (ioctl(descriptor, HIDIOCGRAWINFO, &information) < 0 ||
        information.vendor != 0x046dU) {
        close(descriptor);
        return false;
    }

    if (*feature_id == 0U) {
        uint8_t discovered_index = 0U;
        if (root_feature_index(descriptor, LSM_HIDPP_FEATURE_UNIFIED_BATTERY,
                               &discovered_index, cancel_fd)) {
            *feature_id = LSM_HIDPP_FEATURE_UNIFIED_BATTERY;
            *feature_index = discovered_index;
        }
        if (*feature_id == 0U &&
            root_feature_index(descriptor, LSM_HIDPP_FEATURE_BATTERY_STATUS,
                               &discovered_index, cancel_fd)) {
            *feature_id = LSM_HIDPP_FEATURE_BATTERY_STATUS;
            *feature_index = discovered_index;
        }
    }

    uint8_t response[16] = {0};
    size_t response_size = 0U;
    bool success = false;
    if (*feature_id == LSM_HIDPP_FEATURE_UNIFIED_BATTERY) {
        success = hidpp_request(descriptor, *feature_index,
                                LSM_HIDPP_FUNCTION_UNIFIED_STATUS,
                                NULL, 0U, response, sizeof(response),
                                &response_size, cancel_fd) &&
                  lsm_logitech_hidpp_parse_1004(
                      response, response_size, reading);
    } else if (*feature_id == LSM_HIDPP_FEATURE_BATTERY_STATUS) {
        success = hidpp_request(descriptor, *feature_index,
                                LSM_HIDPP_FUNCTION_GET_STATUS,
                                NULL, 0U, response, sizeof(response),
                                &response_size, cancel_fd) &&
                  lsm_logitech_hidpp_parse_1000(
                      response, response_size, reading);
    }

    if (success) {
        const char *source = *feature_id == LSM_HIDPP_FEATURE_UNIFIED_BATTERY
            ? "Logitech HID++ 0x1004" : "Logitech HID++ 0x1000";
        lsm_copy_string(reading->source, sizeof(reading->source), source);
    }

    close(descriptor);
    return success;
}

static LsmHidppDeviceSlot *find_slot_locked(const char *device_path)
{
    for (size_t index = 0U; index < hidpp_state.count; index++)
        if (strcmp(hidpp_state.devices[index].device_path, device_path) == 0)
            return &hidpp_state.devices[index];
    return NULL;
}

static bool create_cancel_pipe(int descriptors[2])
{
#ifdef O_CLOEXEC
    if (pipe2(descriptors, O_CLOEXEC | O_NONBLOCK) == 0) return true;
#endif
    if (pipe(descriptors) != 0) return false;
    for (size_t index = 0U; index < 2U; index++) {
        const int status_flags = fcntl(descriptors[index], F_GETFL, 0);
        const int descriptor_flags = fcntl(descriptors[index], F_GETFD, 0);
        if (status_flags < 0 || descriptor_flags < 0 ||
            fcntl(descriptors[index], F_SETFL,
                  status_flags | O_NONBLOCK) != 0 ||
            fcntl(descriptors[index], F_SETFD,
                  descriptor_flags | FD_CLOEXEC) != 0) {
            close(descriptors[0]);
            close(descriptors[1]);
            descriptors[0] = -1;
            descriptors[1] = -1;
            return false;
        }
    }
    return true;
}

static void timed_worker_wait_locked(double seconds)
{
    if (seconds < 0.01) seconds = 0.01;
    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) return;
    const time_t whole_seconds = (time_t)seconds;
    deadline.tv_sec += whole_seconds;
    deadline.tv_nsec +=
        (long)((seconds - (double)whole_seconds) * 1000000000.0);
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    (void)pthread_cond_timedwait(&hidpp_state.condition, &hidpp_state.mutex,
                                 &deadline);
}

static ssize_t find_due_slot_locked(double now, double *wait_seconds)
{
    ssize_t due = -1;
    double shortest_wait = LSM_HIDPP_REFRESH_SECONDS;
    for (size_t index = 0U; index < hidpp_state.count; index++) {
        const double remaining =
            hidpp_state.devices[index].next_query_monotonic - now;
        if (remaining <= 0.0) return (ssize_t)index;
        if (remaining < shortest_wait) shortest_wait = remaining;
    }
    if (wait_seconds) *wait_seconds = shortest_wait;
    return due;
}

static void *hidpp_worker(void *user_data)
{
    (void)user_data;
    for (;;) {
        pthread_mutex_lock(&hidpp_state.mutex);
        ssize_t selected = -1;
        while (!hidpp_state.stop_requested && selected < 0) {
            if (hidpp_state.count == 0U) {
                (void)pthread_cond_wait(&hidpp_state.condition,
                                        &hidpp_state.mutex);
                continue;
            }
            double wait_seconds = LSM_HIDPP_REFRESH_SECONDS;
            selected = find_due_slot_locked(monotonic_seconds(),
                                            &wait_seconds);
            if (selected < 0)
                timed_worker_wait_locked(wait_seconds);
        }
        if (hidpp_state.stop_requested) {
            pthread_mutex_unlock(&hidpp_state.mutex);
            break;
        }
        if (selected < 0 || (size_t)selected >= hidpp_state.count) {
            pthread_mutex_unlock(&hidpp_state.mutex);
            continue;
        }

        /* Copy the slot before releasing the mutex. Wireless I/O can take
         * seconds, while topology updates and UI snapshots must stay fast. */
        LsmHidppDeviceSlot request = hidpp_state.devices[selected];
        hidpp_state.devices[selected].next_query_monotonic =
            monotonic_seconds() + LSM_HIDPP_RETRY_SECONDS;
        const int cancel_fd = hidpp_state.cancel_pipe[0];
        pthread_mutex_unlock(&hidpp_state.mutex);

        LsmHidppBatteryReading reading;
        const bool success = query_device(
            request.device_path, &request.feature_id, &request.feature_index,
            &reading, cancel_fd);

        pthread_mutex_lock(&hidpp_state.mutex);
        LsmHidppDeviceSlot *slot = find_slot_locked(request.device_path);
        if (slot && !hidpp_state.stop_requested) {
            const double interval = success
                ? LSM_HIDPP_REFRESH_SECONDS : LSM_HIDPP_RETRY_SECONDS;
            slot->next_query_monotonic = monotonic_seconds() + interval;
            if (success) {
                slot->feature_id = request.feature_id;
                slot->feature_index = request.feature_index;
                slot->reading = reading;
                slot->valid = true;
            } else {
                slot->feature_id = 0U;
                slot->feature_index = 0U;
            }
        }
        pthread_mutex_unlock(&hidpp_state.mutex);
    }
    return NULL;
}

bool lsm_logitech_hidpp_start(void)
{
    pthread_mutex_lock(&hidpp_state.mutex);
    if (hidpp_state.thread_started) {
        pthread_mutex_unlock(&hidpp_state.mutex);
        return true;
    }

    hidpp_state.stop_requested = false;
    hidpp_state.count = 0U;
    memset(hidpp_state.devices, 0, sizeof(hidpp_state.devices));
    if (!create_cancel_pipe(hidpp_state.cancel_pipe)) {
        pthread_mutex_unlock(&hidpp_state.mutex);
        return false;
    }
    const int result = pthread_create(
        &hidpp_state.thread, NULL, hidpp_worker, NULL);
    if (result != 0) {
        close(hidpp_state.cancel_pipe[0]);
        close(hidpp_state.cancel_pipe[1]);
        hidpp_state.cancel_pipe[0] = -1;
        hidpp_state.cancel_pipe[1] = -1;
        pthread_mutex_unlock(&hidpp_state.mutex);
        return false;
    }
    hidpp_state.thread_started = true;
    pthread_mutex_unlock(&hidpp_state.mutex);
    return true;
}

void lsm_logitech_hidpp_set_devices(const char *const *device_paths,
                                    size_t count)
{
    pthread_mutex_lock(&hidpp_state.mutex);
    LsmHidppDeviceSlot updated[LSM_LOGITECH_HIDPP_MAX_DEVICES] = {0};
    size_t updated_count = 0U;
    for (size_t input = 0U;
         input < count && updated_count < LSM_LOGITECH_HIDPP_MAX_DEVICES;
         input++) {
        const char *path = device_paths ? device_paths[input] : NULL;
        if (!path || !path[0]) continue;

        bool duplicate = false;
        for (size_t existing = 0U; existing < updated_count; existing++)
            if (strcmp(updated[existing].device_path, path) == 0) {
                duplicate = true;
                break;
            }
        if (duplicate) continue;

        LsmHidppDeviceSlot *old = find_slot_locked(path);
        if (old) updated[updated_count] = *old;
        else {
            lsm_copy_string(updated[updated_count].device_path,
                            sizeof(updated[updated_count].device_path), path);
            lsm_logitech_hidpp_reset_reading(
                &updated[updated_count].reading);
            updated[updated_count].next_query_monotonic = 0.0;
        }
        updated_count++;
    }
    memcpy(hidpp_state.devices, updated, sizeof(updated));
    hidpp_state.count = updated_count;
    pthread_cond_broadcast(&hidpp_state.condition);
    pthread_mutex_unlock(&hidpp_state.mutex);
}

bool lsm_logitech_hidpp_snapshot(const char *device_path,
                                 LsmHidppBatteryReading *reading)
{
    if (!device_path || !device_path[0] || !reading) return false;
    pthread_mutex_lock(&hidpp_state.mutex);
    const LsmHidppDeviceSlot *slot = find_slot_locked(device_path);
    const bool available = slot && slot->valid;
    if (available) *reading = slot->reading;
    pthread_mutex_unlock(&hidpp_state.mutex);
    return available;
}

void lsm_logitech_hidpp_stop(void)
{
    pthread_mutex_lock(&hidpp_state.mutex);
    if (!hidpp_state.thread_started) {
        pthread_mutex_unlock(&hidpp_state.mutex);
        return;
    }
    hidpp_state.stop_requested = true;
    const uint8_t cancel = 1U;
    const ssize_t cancel_result = write(hidpp_state.cancel_pipe[1],
                                        &cancel, sizeof(cancel));
    /* The condition broadcast is the primary wake-up. A full or already
     * closed cancellation pipe is harmless during shutdown. */
    (void)cancel_result;
    pthread_cond_broadcast(&hidpp_state.condition);
    const pthread_t thread = hidpp_state.thread;
    pthread_mutex_unlock(&hidpp_state.mutex);

    (void)pthread_join(thread, NULL);

    pthread_mutex_lock(&hidpp_state.mutex);
    close(hidpp_state.cancel_pipe[0]);
    close(hidpp_state.cancel_pipe[1]);
    hidpp_state.cancel_pipe[0] = -1;
    hidpp_state.cancel_pipe[1] = -1;
    hidpp_state.thread_started = false;
    hidpp_state.stop_requested = false;
    hidpp_state.count = 0U;
    memset(hidpp_state.devices, 0, sizeof(hidpp_state.devices));
    pthread_mutex_unlock(&hidpp_state.mutex);
}
