# Define the source directory
SRC_DIR := memflow-kmod

# Define the build directory
BUILD_DIR := build

# Define the object files
obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := $(patsubst %.c,%.o,$(wildcard $(SRC_DIR)/*.c))

# Compiler flags
MCFLAGS := -O3
ccflags-y += $(MCFLAGS)
CC += $(MCFLAGS)

# Kernel directory
ifndef KERNELDIR
KDIR := /lib/modules/$(shell uname -r)/build
else
KDIR := $(KERNELDIR)
endif

# Output directory
ifndef OUT_DIR
KOUTPUT := $(PWD)/$(BUILD_DIR)
else
KOUTPUT := $(OUT_DIR)
endif

# Default target
all: $(KOUTPUT)/$(MODULE_NAME).ko

# Build the kernel module
$(KOUTPUT)/$(MODULE_NAME).ko: $(KOUTPUT)
	@echo "Building kernel module in $(KOUTPUT)"
	$(MAKE) -C $(KDIR) M=$(KOUTPUT) src=$(PWD)/$(SRC_DIR) modules
	@echo "Copying object files to $(KOUTPUT)"
	cp $(SRC_DIR)/*.o $(KOUTPUT)/

# Create the build directory
$(KOUTPUT):
	mkdir -p $@

# Clean target
clean:
	$(MAKE) -C $(KDIR) M=$(KOUTPUT) src=$(PWD)/$(SRC_DIR) clean
	rm -rf $(KOUTPUT)

.PHONY: all clean