# Makefile for uci_hostname backend plugin
#
# Cross-compilation with OpenWrt toolchain (Banana Pi R3 = aarch64):
#   make
#
# Override toolchain/sysroot paths if needed:
#   make TOOLCHAIN=/path/to/toolchain SYSROOT=/path/to/sysroot

PLUGIN = uci_hostname.so
SRC    = uci_hostname.c

# Defaults assume the project layout with sysroot/ and adjacent toolchain
TOOLCHAIN ?= $(realpath ../../clixon_openwrt/openwrt-toolchain-25.12.5-mediatek-filogic_gcc-14.3.0_musl.Linux-x86_64/toolchain-aarch64_cortex-a53_gcc-14.3.0_musl)
SYSROOT   ?= $(realpath ../sysroot)

CC       = $(TOOLCHAIN)/bin/aarch64-openwrt-linux-musl-gcc
CFLAGS  += -fPIC -Wall -I$(SYSROOT)/usr/include
LDFLAGS += -shared -Wl,--unresolved-symbols=ignore-in-object-files

export STAGING_DIR = $(TOOLCHAIN)/..

.PHONY: all clean install

all: $(PLUGIN)

$(PLUGIN): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

install: $(PLUGIN)
	install -d $(DESTDIR)/usr/lib/clixon/backend
	install -m 755 $(PLUGIN) $(DESTDIR)/usr/lib/clixon/backend/

clean:
	rm -f $(PLUGIN)
