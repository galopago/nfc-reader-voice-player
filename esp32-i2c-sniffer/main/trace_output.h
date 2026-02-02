/*
 * trace_output.h
 * Single point for emitting sniffer traces (UART today; WiFi/CDC later for MCP).
 * Do not scatter printf() elsewhere; use trace_send / trace_send_line only.
 */
#ifndef TRACE_OUTPUT_H
#define TRACE_OUTPUT_H

#include <stdarg.h>

/* Send a formatted line (adds newline). Uses UART. */
void trace_send_line(const char *fmt, ...);

/* Send raw buffer (no newline). For future binary or chunked output. */
void trace_send_buf(const char *buf, unsigned len);

#endif /* TRACE_OUTPUT_H */
