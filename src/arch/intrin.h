#pragma once

#include <stdint.h>

///////////////////////////////////////////////////////////////
// Arch specific intrinsics
///////////////////////////////////////////////////////////////

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

static inline void __invlpg(void* m) {
    asm volatile("invlpg (%0)" : : "b"(m) : "memory");
}

static inline unsigned long __readcr0(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr0, %[value]" : [value] "=q"(value));
    return value;
}

static inline unsigned long __readcr2(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr2, %[value]" : [value] "=q"(value));
    return value;
}

static inline unsigned long __readcr3(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr3, %[value]" : [value] "=q"(value));
    return value;
}

static inline unsigned long __readcr4(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr4, %[value]" : [value] "=q"(value));
    return value;
}

static inline unsigned long __readcr8(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr8, %[value]" : [value] "=q"(value));
    return value;
}

static inline void __writecr0(const unsigned long long Data) {
    __asm__ __volatile__("mov %[Data], %%cr0" : : [Data] "q"(Data) : "memory");
}

static inline void __writecr3(const unsigned long long Data) {
    __asm__ __volatile__("mov %[Data], %%cr3" : : [Data] "q"(Data) : "memory");
}

static inline void __writecr4(const unsigned long long Data) {
    __asm__ __volatile__("mov %[Data], %%cr4" : : [Data] "q"(Data) : "memory");
}

static inline void __writecr8(const unsigned long long Data) {
    __asm__ __volatile__("mov %[Data], %%cr8" : : [Data] "q"(Data) : "memory");
}

static inline void cpu_relax() {
    __builtin_ia32_pause();
}
