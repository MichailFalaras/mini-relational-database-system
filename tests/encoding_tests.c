#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "encoding.h"

static int test_unsigned(void) {
    uint8_t bytes[8] = {0};

    store_u8_le(bytes, 0xABu);
    assert(bytes[0] == 0xABu && load_u8_le(bytes) == 0xABu);

    store_u16_le(bytes, 0xAABBu);
    assert(bytes[0] == 0xBBu && bytes[1] == 0xAAu);
    assert(load_u16_le(bytes) == 0xAABBu);

    store_u32_le(bytes, UINT32_C(0xAABBCCDD));
    const uint8_t expected32[] = {0xDD, 0xCC, 0xBB, 0xAA};
    assert(memcmp(bytes, expected32, sizeof(expected32)) == 0);
    assert(load_u32_le(bytes) == UINT32_C(0xAABBCCDD));

    store_u64_le(bytes, UINT64_C(0x1122334455667788));
    const uint8_t expected64[] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    assert(memcmp(bytes, expected64, sizeof(expected64)) == 0);
    assert(load_u64_le(bytes) == UINT64_C(0x1122334455667788));

    return 0;
}

static int test_signed(void) {
    uint8_t bytes[8] = {0};

    store_i8_le(bytes, INT8_C(-2));
    assert(bytes[0] == 0xFEu && load_i8_le(bytes) == -2);

    store_i16_le(bytes, INT16_C(-2));
    assert(bytes[0] == 0xFEu && bytes[1] == 0xFFu && load_i16_le(bytes) == -2);

    store_i32_le(bytes, INT32_C(-2));
    const uint8_t expected32[] = {0xFE, 0xFF, 0xFF, 0xFF};
    assert(memcmp(bytes, expected32, 4) == 0 && load_i32_le(bytes) == -2);

    store_i64_le(bytes, INT64_C(-2));
    const uint8_t expected64[] = {0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    assert(memcmp(bytes, expected64, 8) == 0 && load_i64_le(bytes) == -2);

    return 0;
}

static int test_floats(void) {
    uint8_t bytes[8] = {0};

    store_float_le(bytes, 1.0f);
    const uint8_t expected_float[] = {0x00, 0x00, 0x80, 0x3F};
    assert(memcmp(bytes, expected_float, 4) == 0);
    assert(load_float_le(bytes) == 1.0f);

    store_double_le(bytes, -2.5);
    const uint8_t expected_double[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xC0};
    assert(memcmp(bytes, expected_double, 8) == 0);
    assert(load_double_le(bytes) == -2.5);

    return 0;
}

static int test_repeated_values(void) {
    uint32_t seed = UINT32_C(0x10203040);
    uint8_t bytes[4];
    
    for (int i = 0; i < 10000; i++) {
        seed = seed * UINT32_C(1664525) + UINT32_C(1013904223);
        store_u32_le(bytes, seed);
        assert(bytes[0] == (uint8_t)seed);
        assert(bytes[1] == (uint8_t)(seed >> 8));
        assert(bytes[2] == (uint8_t)(seed >> 16));
        assert(bytes[3] == (uint8_t)(seed >> 24));
        assert(load_u32_le(bytes) == seed);
    }

    return 0;
}

/* ---------- Logging Helper ---------- */

void generate_output(int result, int test_num, char *test_desc) {
    int space = 40 - (int) strlen(test_desc);
    char *result_str = result == 0 ? "SUCCESS" : "ERROR";

    printf("TEST[%d]: %s - %*s\n", test_num, test_desc, space, result_str);
}

int main(int argc, char *argv[]) {
    int result;

    result = test_unsigned();
    generate_output(result, 0, "test_unsigned");
    result = test_signed();
    generate_output(result, 1, "test_signed");
    result = test_floats();
    generate_output(result, 2, "test_floats");
    result = test_repeated_values();
    generate_output(result, 3, "test_repeated_values");

    return 0;
}