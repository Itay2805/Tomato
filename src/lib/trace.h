#pragma once

#define DEBUG(fmt, ...) trace("[?] " fmt "\n", ##__VA_ARGS__)
#define TRACE(fmt, ...) trace("[*] " fmt "\n", ##__VA_ARGS__)
#define WARN(fmt, ...)  trace("[!] " fmt "\n", ##__VA_ARGS__)
#define ERROR(fmt, ...) trace("[-] " fmt "\n", ##__VA_ARGS__)

/**
 * The kernel tracing function, outputs to the debug console,
 * whatever it may be
 */
[[gnu::format(printf, 1, 2)]]
void trace(const char* fmt, ...);
