/*
 * trace_output.c
 * All sniffer output goes through here. Uses default console (stdout) so that
 * on ESP32-C3 with USB-Serial/JTAG traces appear in the same monitor as logs.
 * Replace with socket/CDC later for MCP if needed.
 */
#include "trace_output.h"
#include <stdio.h>
#include <string.h>

#define TRACE_BUF_SIZE   256

static void trace_console_write(const char *buf, unsigned len)
{
    fwrite(buf, 1, len, stdout);
    fflush(stdout);
}

void trace_send_line(const char *fmt, ...)
{
    char buf[TRACE_BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        unsigned len = (unsigned)n;
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        if (len > 0 && buf[len - 1] != '\n') {
            buf[len] = '\n';
            len++;
        }
        trace_console_write(buf, len);
    }
}

void trace_send_buf(const char *buf, unsigned len)
{
    if (buf && len) trace_console_write(buf, len);
}
