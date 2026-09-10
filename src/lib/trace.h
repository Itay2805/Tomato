#pragma once

#define DEBUG(fmt, ...) _trace("[?] " fmt "\n", ##__VA_ARGS__)
#define TRACE(fmt, ...) _trace("[*] " fmt "\n", ##__VA_ARGS__)
#define WARN(fmt, ...)  _trace("[!] " fmt "\n", ##__VA_ARGS__)
#define ERROR(fmt, ...) _trace("[-] " fmt "\n", ##__VA_ARGS__)

/**
 * The kernel tracing function, outputs to the debug console,
 * whatever it may be
 */
[[gnu::format(printf, 1, 2)]]
void _trace(const char* fmt, ...);
