// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file metric_format.h
 * @brief Standard formatting policy for optional monitoring values.
 *
 * Availability is always explicit: a numeric zero is rendered only when the
 * collector marked the metric available; otherwise the result is "N/A".
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_METRIC_FORMAT_H
#define LINUX_SYSTEM_MONITOR_METRIC_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Format memory with the Memory page's fixed one-decimal GB convention.
 *
 * @param [in] bytes Quantity in bytes.
 * @param [out] buffer Caller-owned output buffer.
 * @param [in] size Capacity of @p buffer in bytes.
 * @return @p buffer for expression chaining.
 */
char *lsm_metric_format_memory_gb(uint64_t bytes, char *buffer, size_t size);
/**
 * Format a compact binary capacity suitable for device subtitles.
 *
 * @param [in] bytes Capacity in bytes.
 * @param [out] buffer Caller-owned output buffer.
 * @param [in] size Capacity of @p buffer in bytes.
 * @return @p buffer for expression chaining.
 */
char *lsm_metric_format_disk_capacity(uint64_t bytes, char *buffer, size_t size);
/**
 * Format a network byte quantity using decimal network-unit steps.
 *
 * Network presentation begins at KB or Kb and scales by 1000, matching
 * conventional network-rate notation while storage and memory remain base-2.
 *
 * @param [in] bytes Byte quantity or byte-per-second rate.
 * @param [in] use_bits Whether to convert bytes to bits before scaling.
 * @param [in] per_second Whether to append the per-second suffix.
 * @param [out] buffer Caller-owned output buffer.
 * @param [in] size Capacity of @p buffer in bytes.
 * @return @p buffer for expression chaining.
 */
char *lsm_metric_format_network(long double bytes, bool use_bits,
                                bool per_second, char *buffer, size_t size);
/**
 * Format send and receive rates on one compact line using one shared unit.
 *
 * The larger of the two rates chooses one decimal display unit so the pair
 * remains directly comparable and avoids repeating the suffix in the compact
 * Performance sidebar.
 *
 * @param [in] send_bytes Send rate in bytes per second.
 * @param [in] receive_bytes Receive rate in bytes per second.
 * @param [in] use_bits Whether to convert both rates to bits before scaling.
 * @param [out] buffer Caller-owned output buffer.
 * @param [in] size Capacity of @p buffer in bytes.
 * @return @p buffer containing a compact S/R rate pair.
 */
char *lsm_metric_format_network_pair(long double send_bytes,
                                     long double receive_bytes, bool use_bits,
                                     char *buffer, size_t size);
/**
 * Format a negotiated network link rate reported in decimal Mb/s.
 *
 * The backend's decimal-Mb/s value is preserved as a decimal network rate.
 * Values below 1 Mb/s are shown as Kb/s; values at 1000 Mb/s and above are
 * promoted through Gb/s and Tb/s without binary rescaling.
 *
 * @param [in] megabits_per_second Negotiated decimal-Mb/s rate from backend.
 * @param [out] buffer Caller-owned output buffer.
 * @param [in] size Capacity of @p buffer in bytes.
 * @return @p buffer containing Kb/s, Mb/s, Gb/s, Tb/s, or "N/A".
 */
char *lsm_metric_format_link_speed_mbps(double megabits_per_second,
                                        char *buffer, size_t size);
/**
 * Format an optional percentage while preserving a measured zero.
 *
 * @param [in] available Whether @p value is a valid current sample.
 * @param [in] value Percentage value when available.
 * @param [out] buffer Caller-owned output buffer.
 * @param [in] size Capacity of @p buffer in bytes.
 * @return @p buffer containing a percentage or "N/A".
 */
char *lsm_metric_format_percent(bool available, double value,
                                char *buffer, size_t size);
/**
 * Format an optional frequency in megahertz.
 *
 * @param [in] available Whether @p value is a valid current sample.
 * @param [in] value Frequency in MHz.
 * @param [out] buffer Caller-owned output buffer.
 * @param [in] size Capacity of @p buffer in bytes.
 * @return @p buffer containing a frequency or "N/A".
 */
char *lsm_metric_format_mhz(bool available, double value,
                            char *buffer, size_t size);
/**
 * Format an optional frequency in gigahertz with two decimal places.
 *
 * @param [in] available Whether @p value is a valid current sample.
 * @param [in] value Frequency in GHz.
 * @param [out] buffer Caller-owned output buffer.
 * @param [in] size Capacity of @p buffer in bytes.
 * @return @p buffer containing a frequency or "N/A".
 */
char *lsm_metric_format_ghz(bool available, double value,
                            char *buffer, size_t size);
/**
 * Format an optional Celsius temperature.
 *
 * @param [in] available Whether @p value is a valid current sample.
 * @param [in] value Temperature in degrees Celsius.
 * @param [out] buffer Caller-owned output buffer.
 * @param [in] size Capacity of @p buffer in bytes.
 * @return @p buffer containing a temperature or "N/A".
 */
char *lsm_metric_format_celsius(bool available, double value,
                                char *buffer, size_t size);
/**
 * Format an optional power measurement in watts.
 *
 * @param [in] available Whether @p value is a valid current sample.
 * @param [in] value Power in watts.
 * @param [out] buffer Caller-owned output buffer.
 * @param [in] size Capacity of @p buffer in bytes.
 * @return @p buffer containing a power value or "N/A".
 */
char *lsm_metric_format_watts(bool available, double value,
                              char *buffer, size_t size);

#endif
