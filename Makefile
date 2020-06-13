TARGET_EXEC ?= a.out

GCC ?= /usr/local/bin/g++-9
BUILD_DIR ?= ./build
SRC_DIRS ?= ./src

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS ?= $(INC_FLAGS) -MMD -MP
CFLAGS ?= -std=gnu99

# assembly
$(BUILD_DIR)/%.s.o: %.s
	$(MKDIR_P) $(dir $@)
		$(AS) $(ASFLAGS) -c $< -o $@

# c source
# cc  -I./src -MMD -MP   myserver.c   -o myserver
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(GCC) $(CFLAGS) -c $< -o $@

# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(GCC) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

%: %.cpp
	$(MKDIR_P) $(dir $@)
	$(GCC) -o $@ $<
	chmod +x $@

.PHONY: clean

clean:
		$(RM) -r $(BUILD_DIR)

MKDIR_P ?= mkdir -p
