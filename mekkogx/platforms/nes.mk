# NES: a cart image (.nes) for the FujiNet NES cartridge. There is no disk:
# the image is the program, and the FujiNet pushes it to the cartridge from
# the NES CONFIG's file list. The project lays out the banks and the "FUJI"
# claim in its own linker config (LDFLAGS_EXTRA_NES), and checks the image
# after the link (its nes/executable-post).

EXEC_SUFFIX = .nes
LIBRARY = $(R2R_PD)/$(PRODUCT_BASE).$(PLATFORM).lib

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk
include $(MWD)/toolchains/cc65.mk

r2r:: $(BUILD_EXEC) $(BUILD_LIB) $(R2R_EXTRA_DEPS)
	make -f $(PLATFORM_MK) $(PLATFORM)/r2r-post
