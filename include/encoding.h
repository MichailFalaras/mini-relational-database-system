#ifndef ENCODING_H_
#define ENCODING_H_

#include <stdint.h>
#include <string.h>

/* Statically assert both float and double are 4 and 8 bytes accordingly.
 * It doesn't verify a certain architecture.
 * Static check, it fails in compilation. */
#include <assert.h>
static_assert(sizeof(float) == 4);
static_assert(sizeof(double) == 8);

/* NOTE: TO BE IMPLEMENTED SOON TO PROTECT DATA FROM LOADING/STORING
 *
 * EncodingByteReader helps solve reading out of bounds data of varying sizes
 * by giving the estimated remaining size of the buffer we're reading from. */
typedef struct encoding_byte_reader {
    const uint8_t *src;
    uint32_t remaining;
} EncodingByteReader;

/* Solving Portability by storing/serializing data in Little-Endian format. */

static inline uint8_t load_u8_le(const uint8_t *src) {
    return (uint8_t)src[0];
}

static inline void store_u8_le(uint8_t *dst, uint8_t value) {
    dst[0] = (uint8_t)value;
}

static inline uint16_t load_u16_le(const uint8_t *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static inline void store_u16_le(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
}

static inline uint32_t load_u32_le(const uint8_t *src) {
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) | 
           ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

static inline void store_u32_le(uint8_t *dst, uint32_t value) {
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

static inline uint64_t load_u64_le(const uint8_t *src) {
    return (uint64_t)src[0] | ((uint64_t)src[1] << 8) | 
           ((uint64_t)src[2] << 16) | ((uint64_t)src[3] << 24) |
           ((uint64_t)src[4] << 32) | ((uint64_t)src[5] << 40) |
           ((uint64_t)src[6] << 48) | ((uint64_t)src[7] << 56);
}

static inline void store_u64_le(uint8_t *dst, uint64_t value) {
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
    dst[4] = (uint8_t)(value >> 32);
    dst[5] = (uint8_t)(value >> 40);
    dst[6] = (uint8_t)(value >> 48);
    dst[7] = (uint8_t)(value >> 56);
}

/* Signed and unsigned versions are interpretations of the bits
 * being loaded/stored. Therefore, there's no need to implement different helpers. */
static inline int8_t load_i8_le(const uint8_t *src) {
    return (int8_t) src[0];
}

static inline void store_i8_le(uint8_t *dst, int8_t value) {
    store_u8_le(dst, (uint8_t) value);
}

static inline int16_t load_i16_le(const uint8_t *src) {
    return (int16_t) load_u16_le(src);
}

static inline void store_i16_le(uint8_t *dst, int16_t value) {
    store_u16_le(dst, (uint16_t) value);
}

static inline int32_t load_i32_le(const uint8_t *src) {
    return (int32_t) load_u32_le(src);
}

static inline void store_i32_le(uint8_t *dst, int32_t value) {
    store_u32_le(dst, (uint32_t) value);
}

static inline int64_t load_i64_le(const uint8_t *src) {
    return (int64_t) load_u64_le(src);
}

static inline void store_i64_le(uint8_t *dst, int64_t value) {
    store_u64_le(dst, (uint64_t) value);
}

static inline float load_float_le(const uint8_t *src) {
    
    /* Get raw bits in 32 bits. */
    uint32_t bits = load_u32_le(src);

    /* Copy them over to a float to get actual value. */
    float float_val = 0.0f;
    memcpy(&float_val, &bits, sizeof(uint32_t));
    
    return float_val;
}

static inline void store_float_le(uint8_t *dst, float value) {

    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof(uint32_t));

    store_u32_le(dst, bits);
}

static inline double load_double_le(const uint8_t *src) {

    uint64_t bits = load_u64_le(src);

    double double_val = 0.0;
    memcpy(&double_val, &bits, sizeof(uint64_t));

    return double_val;
}

static inline void store_double_le(uint8_t *dst, double value) {

    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof(uint64_t));

    store_u64_le(dst, bits);
}

#endif