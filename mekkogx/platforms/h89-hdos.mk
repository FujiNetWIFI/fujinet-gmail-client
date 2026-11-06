EXEC_SUFFIX = .abs
DISK = $(R2R_PD)/$(PRODUCT_BASE).h8d
LIBRARY = $(R2R_PD)/$(PRODUCT_BASE).$(PLATFORM).lib
DISK_TOOL = z88dk-appmake
DISK_TOOL_INFO = https://github.com/z88dk/z88dk

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk
include $(MWD)/toolchains/z88dk.mk

MSX_FLAGS = +hdos -create-app -startup=3 -Cz--abs -pragma-define:CLIB_DISABLE_FGETS_CURSOR=1
CFLAGS += $(MSX_FLAGS) -D__H89__
LDFLAGS += $(MSX_FLAGS)

r2r:: $(BUILD_DISK) $(BUILD_EXEC) $(BUILD_LIB) $(R2R_EXTRA_DEPS)
	make -f $(PLATFORM_MK) $(PLATFORM)/r2r-post

# A 100K H17 disk holding the program: MOUNT SY1: then RUN SY1:<NAME>.ABS.
# appmake +hdos puts just the one file on it, as it is (--dumb): the .ABS
# the link made, header and all, and whatever was packed after it. Its
# directory entry then needs hdos-dir-fixup.py.
DISK_BOOT_NAME = $(shell echo $(basename $(notdir $(DISK_BOOT_EXEC))) | tr '[:lower:]' '[:upper:]')

$(BUILD_DISK): $(DISK_BOOT_EXEC) $(DISK_EXTRA_DEPS) | $(R2R_PD) $(CACHE_PLATFORM)
	$(call require,$(DISK_TOOL),$(DISK_TOOL_INFO))
	cp $(DISK_BOOT_EXEC) $(CACHE_PLATFORM)/disk.bin
	cd $(CACHE_PLATFORM) && $(RM) disk.h8d && $(DISK_TOOL) +hdos --dumb -b disk.bin -o $(DISK_BOOT_NAME).ABS
	$(MWD)/hdos-dir-fixup.py $(CACHE_PLATFORM)/disk.h8d $(DISK_BOOT_NAME).ABS $(DISK_BOOT_EXEC)
	mv $(CACHE_PLATFORM)/disk.h8d $@
	make -f $(PLATFORM_MK) $(PLATFORM)/disk-post
