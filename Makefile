# Variables
CC := gcc
# Automatically generate dependecy files (.d)
CFLAGS := -Wall -fsanitize=address -MMD -MP
CPPFLAGS := -I./include

# Directories
SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin
TESTS_DIR := tests

# Convert list of src files to object files
SRC := $(wildcard $(SRC_DIR)/btree/*.c) \
		$(wildcard $(SRC_DIR)/constraints/*.c) \
		$(wildcard $(SRC_DIR)/data_types/*.c) \
		$(wildcard $(SRC_DIR)/execution_engine/*.c) \
		$(wildcard $(SRC_DIR)/expressions/*.c) \
		$(wildcard $(SRC_DIR)/index/*.c) \
		$(wildcard $(SRC_DIR)/pager/*.c) \
		$(wildcard $(SRC_DIR)/row/*.c) \
		$(wildcard $(SRC_DIR)/schema/*.c) \
		$(wildcard $(SRC_DIR)/serialize/*.c) \
		$(wildcard $(SRC_DIR)/table/*.c) \
		$(wildcard $(SRC_DIR)/tokenizer/*.c)

OBJ := $(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TESTS := $(wildcard $(TESTS_DIR)/*.c)
# Dependency files (.d)
DEP := $(OBJ:.o=.d)

# Create all object files 
all: $(OBJ)

# Create single test executable and link all object files
$(BUILD_DIR)/%: $(TESTS_DIR)/%.c $(OBJ)
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJ) -o $@

# Pattern rule / Object file creation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ 

# Include automatically generated dependency files
-include $(DEP)

clean: 
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)

.PHONY: all clean