CC ?= cc
CPPFLAGS ?= -I../src
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
BUILD_DIR ?= build

TEST_BINARY := $(BUILD_DIR)/test_mxw01_protocol

.PHONY: test clean

test: $(TEST_BINARY)
	$(TEST_BINARY)

$(BUILD_DIR):
	mkdir -p $@

$(TEST_BINARY): test_mxw01_protocol.c ../src/mxw01_protocol.c ../src/mxw01_protocol.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_mxw01_protocol.c ../src/mxw01_protocol.c -o $@

clean:
	$(RM) -r $(BUILD_DIR)
