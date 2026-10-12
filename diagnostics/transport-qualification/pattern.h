#ifndef FNR_QUALIFICATION_PATTERN_H
#define FNR_QUALIFICATION_PATTERN_H
#include <stdint.h>
#include <stddef.h>
/* Diagnostic-only opaque bytes. No receiver envelope or HID semantics. */
static const uint8_t qualification_source[6] = {2, 5, 17, 34, 51, 68};
static const uint8_t qualification_telemetry_source[6] = {2, 5, 17, 34, 51, 254};
static inline uint8_t qualification_byte(uint32_t id, size_t offset) {
    if (offset < 4) return (uint8_t)(id >> (8u * offset));
    return (uint8_t)((id * 37u + (uint32_t)offset * 73u) ^ ((uint32_t)offset >> 1u));
}
static inline uint64_t qualification_hash(uint64_t hash, const uint8_t *bytes, size_t count) {
    for (size_t i = 0; i < count; ++i) hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    return hash;
}
static inline void qualification_put32(uint8_t *bytes, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) bytes[i] = (uint8_t)(value >> (8u * i));
}
static inline uint32_t qualification_get32(const uint8_t *bytes) {
    uint32_t result = 0;
    for (size_t i = 0; i < 4; ++i) result |= (uint32_t)bytes[i] << (8u * i);
    return result;
}
#endif
