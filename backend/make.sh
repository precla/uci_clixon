make CC=aarch64-openwrt-linux-musl-gcc \
       CFLAGS="-I$(STAGING_DIR)/usr/include" \
       LDFLAGS="-L$(STAGING_DIR)/usr/lib"
