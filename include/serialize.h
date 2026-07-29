#ifndef SERIALIZE_H_
#define SERIALIZE_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct value Value;

extern bool serialize_cell_data(void *page_data, uint16_t offset, void *payload, Value **row_keys, void *context);

extern bool serialize_value_data(Value *value, void *serialized_output);

#endif