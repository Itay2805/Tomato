#pragma once

#include <stdint.h>

#ifdef __x86_64__
    #include <x86intrin.h>

static inline uint8_t __inbyte(uint16_t port) {
    uint8_t byte;
    asm volatile("inb %w[port], %b[byte]" : [byte] "=a"(byte) : [port] "Nd"(port));
    return byte;
}

static inline uint16_t __inword(uint16_t port) {
    uint16_t word;
    asm volatile("inw %w[port], %w[word]" : [word] "=a"(word) : [port] "Nd"(port));
    return word;
}

static inline uint32_t __indword(uint16_t port) {
    uint32_t dword;
    asm volatile("inl %w[port], %k[dword]" : [dword] "=a"(dword) : [port] "Nd"(port));
    return dword;
}

static inline void __outbyte(uint16_t port, uint8_t data) {
    asm volatile("outb %b[data], %w[port]" : : [port] "Nd"(port), [data] "a"(data));
}

static inline void __outword(uint16_t port, uint16_t data) {
    asm volatile("outw %w[data], %w[port]" : : [port] "Nd"(port), [data] "a"(data));
}

static inline void __outdword(uint16_t port, uint32_t data) {
    asm volatile("outl %k[data], %w[port]" : : [port] "Nd"(port), [data] "a"(data));
}

static inline void irq_enable() {
    asm("sti");
}
static inline void irq_disable() {
    asm("cli");
}

#else
    #error Unknown arch
#endif
