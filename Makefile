# Top-level Makefile for uci_hostname plugins
#
# Builds both the backend plugin and the CLI plugin:
#   make               # build all plugins
#   make clean
#
# Each sub-Makefile cross-compiles using its own default toolchain/sysroot
# paths. Override both here if needed:
#   make TOOLCHAIN=/path/to/toolchain SYSROOT=/path/to/sysroot

.PHONY: all clean

# Forward TOOLCHAIN/SYSROOT only if given on the command line, so the
# sub-Makefiles fall back to their own defaults when they are not set.
ifdef TOOLCHAIN
TOOLCHAIN_ARG = TOOLCHAIN=$(TOOLCHAIN)
endif
ifdef SYSROOT
SYSROOT_ARG = SYSROOT=$(SYSROOT)
endif

all:
	$(MAKE) -C backend $(TOOLCHAIN_ARG) $(SYSROOT_ARG)
	$(MAKE) -C cli     $(TOOLCHAIN_ARG) $(SYSROOT_ARG)

clean:
	$(MAKE) -C backend clean
	$(MAKE) -C cli     clean