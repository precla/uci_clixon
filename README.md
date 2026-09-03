# uci_hostname — Clixon plugin for OpenWrt hostname

A Clixon backend plugin that bridges the YANG `ietf-system` model to OpenWrt's
UCI system. Changes to `/system/hostname` over NETCONF / RESTCONF / CLI are
pushed to `uci set system.@system[0].hostname=...` and applied immediately.

Tested on a **Banana Pi R3** (aarch64, MediaTek Filogic 830) running OpenWrt with `clixon 7.8.0`.

## Project layout

```
uci_hostname/
├── backend/
│   ├── Makefile             # cross-compile rules
│   └── uci_hostname.c       # the plugin (commit + statedata callbacks)
├── clispec/
│   └── system.cli           # CLI commands (auto-generated from YANG)
├── config/
│   └── clixon.xml           # clixon backend configuration
├── yang/
│   ├── iana-crypt-hash@2014-08-06.yang
│   ├── ietf-system@2014-08-06.yang
│   ├── ietf-yang-types@2013-07-15.yang
│   └── ietf-interfaces@2018-02-20.yang
├── sysroot/                 # generated headers for cross-compiling (see below)
├── deploy.sh                # push everything to the router
└── README.md                # this file
```

## Prerequisites

### 1. OpenWrt toolchain

Download the prebuilt toolchain matching your OpenWrt version / target.
For a BPI-R3 running OpenWrt 25.12.2 (mediatek/filogic):

```
https://downloads.openwrt.org/releases/<version>/targets/mediatek/filogic/
```

Look for `openwrt-toolchain-*_gcc-*_musl.Linux-x86_64.tar.zst`.
Extract it somewhere — the path used in `backend/Makefile` is:

```
../clixon_openwrt/openwrt-toolchain-25.12.5-mediatek-filogic_gcc-14.3.0_musl.Linux-x86_64/
  toolchain-aarch64_cortex-a53_gcc-14.3.0_musl/
```

Override with `make TOOLCHAIN=/your/path` if it lives elsewhere.

### 2. Clixon headers (sysroot)

The OpenWrt clixon runtime package has `.so` libraries but **no headers**.
The plugin is built against headers generated from the Clixon/cligen **source**
matching the clixon version on your router. For clixon **7.8.0**:

```sh
# Download sources
curl -sL https://codeload.github.com/clicon/clixon/tar.gz/7.8.0 | tar xz -C /tmp/
curl -sL https://codeload.github.com/clicon/cligen/tar.gz/7.8.0 | tar xz -C /tmp/

# Build sysroot
SYSROOT=$(pwd)/sysroot
mkdir -p $SYSROOT/usr/include/{clixon,cligen} $SYSROOT/usr/lib

cp /tmp/cligen-7.8.0/*.h                        $SYSROOT/usr/include/cligen/
cp /tmp/clixon-7.8.0/lib/clixon/clixon_*.h      $SYSROOT/usr/include/clixon/
cp /tmp/clixon-7.8.0/apps/backend/clixon_backend_*.h $SYSROOT/usr/include/clixon/
cp /tmp/clixon-7.8.0/include/clixon_custom.h    $SYSROOT/usr/include/clixon/
```

Generate `clixon.h` (from `clixon.h.in`) with version defines:

```sh
sed -e 's/#undef CLIXON_VERSION_MAJOR/#define CLIXON_VERSION_MAJOR 7/' \
    -e 's/#undef CLIXON_VERSION_MINOR/#define CLIXON_VERSION_MINOR 8/' \
    -e 's/#undef CLIXON_VERSION_PATCH/#define CLIXON_VERSION_PATCH 0/' \
    /tmp/clixon-7.8.0/lib/clixon/clixon.h.in \
  > $SYSROOT/usr/include/clixon/clixon.h
```

Generate `clixon_config.h` (from `clixon_config.h.in`) — every `#undef FOO`
that isn't one of the required defines below becomes `/* #undef FOO */`:

```sh
python3 - <<'PY'
import re, os
sysroot = os.environ.get('SYSROOT', 'sysroot')
defines = {
    'CAT_BIN': '"/bin/cat"',
    'CLIXON_CONFIG_BINDIR': '"/usr/bin"',
    'CLIXON_CONFIG_LOCALSTATEDIR': '"/var"',
    'CLIXON_CONFIG_SBINDIR': '"/usr/sbin"',
    'CLIXON_CONFIG_SYSCONFDIR': '"/etc"',
    'CLIXON_DEFAULT_CONFIG': '"/etc/clixon/clixon.xml"',
    'CLIXON_VERSION_MAJOR': '7',
    'CLIXON_VERSION_MINOR': '8',
    'CLIXON_VERSION_PATCH': '0',
    'HAVE_LIBNGHTTP2': '1',
}
with open('/tmp/clixon-7.8.0/include/clixon_config.h.in') as f:
    src = f.read()
out = []
for line in src.split('\n'):
    m = re.match(r'^#undef\s+(\w+)', line)
    if m and m.group(1) in defines:
        out.append(f'#define {m.group(1)} {defines[m.group(1)]}')
    elif m:
        out.append(f'/* #undef {m.group(1)} */')
    else:
        out.append(line)
with open(f'{sysroot}/usr/include/clixon/clixon_config.h', 'w') as f:
    f.write('\n'.join(out))
PY
```

The resulting `sysroot/` contains everything needed to cross-compile.

## Build

```sh
cd backend
make
```

Defaults in the Makefile expect the toolchain next to the project (see the
layout above). Override if needed:

```sh
make TOOLCHAIN=/path/to/toolchain-aarch64_cortex-a53_gcc-14.3.0_musl \
     SYSROOT=/path/to/sysroot
```

The build produces `backend/uci_hostname.so` — a 64-bit aarch64 shared object.
Clixon/cligen symbols are left unresolved at link time
(`-Wl,--unresolved-symbols=ignore-in-object-files`); they bind at runtime when
`clixon_backend` loads the plugin via `dlopen`.

Verify:

```sh
file backend/uci_hostname.so
# -> ELF 64-bit LSB shared object, ARM aarch64, ...
```

## Deploy

```sh
./deploy.sh <router-ip>
```

The script:

1. Opens a single SSH `ControlMaster` connection — password entered once.
2. Stops and disables the packaged `/etc/init.d/clixon` procd service (so it
   doesn't respawn the backend with the old hello config).
3. Kills any running `clixon_backend`.
4. Pushes files via `scp -O` (legacy protocol — dropbear has no `sftp-server`):
   - YANG models → `/usr/share/clixon/`
   - `system.cli` → `/usr/lib/clixon/clispec/`
   - `clixon.xml` → `/etc/clixon/clixon.xml`
   - `uci_hostname.so` → `/usr/lib/clixon/backend/`
5. Clears the old datastore at `/var/clixon/`.
6. Starts `clixon_backend -s init -l s` in daemon mode.
7. Prints a verification listing and the backend PID.

## Test

### Via CLI

```sh
ssh root@<router-ip> clixon_cli
```

```
system> set system hostname OWRTR3
system> commit
system> quit
```

### Verify on the router

```sh
uci get system.@system[0].hostname   # -> OWRTR3
cat /proc/sys/kernel/hostname        # -> OWRTR3
logread | grep -i hostname | tail    # -> "apply_hostname: setting hostname to 'OWRTR3'"
```

A fresh shell prompt will show the new hostname (`root@OWRTR3:~#`).

### Via NETCONF / RESTCONF

Any standard NETCONF `edit-config` targeting `/system/hostname` or RESTCONF
`PUT /data/ietf-system:system/hostname` triggers the same commit callback.

## How it works

- **Auto-CLI for multiple modules** — `CLICON_YANG_MODULE_MAIN=ietf-system`
  only loads `ietf-system` plus its *imported* dependencies. To also expose
  `ietf-interfaces` in the CLI, `CLICON_YANG_MAIN_DIR=/usr/share/clixon` loads
  **all** `.yang` files in that directory. The `<autocli>` rules then enable
  `set/merge/create/delete interfaces ...` for `ietf-interfaces` while
  `module-default=false` keeps unrelated packaged modules out of the CLI.
- **`uci_hostname_commit`** — on `commit`, extracts `/system/hostname` from
  the target datastore via `xpath_first(target, NULL, "system/hostname")`,
  then runs `uci set`, `uci commit system`, and writes
  `/proc/sys/kernel/hostname`.
- **`uci_hostname_statedata`** — populates `/system-state/platform` from
  `/etc/openwrt_release` and `/proc/sys/kernel/hostname` for GET requests.
- **`clixon_plugin_init`** — registers the callbacks; single-arg signature
  `(clixon_handle h)` as required by clixon 7.8.0.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `./deploy.sh` hangs at `Stopping clixon backend` | procd respawning — the script runs `/etc/init.d/clixon stop && disable` for this reason. |
| `scp: /usr/libexec/sftp-server: not found` | Dropbear on OpenWrt has no SFTP. The script uses `scp -O` (legacy protocol). |
| CLI prompt is `cli>` not `system>` | The packaged hello config is still in `/etc/clixon/clixon.xml`. Re-run `./deploy.sh` — it now correctly overwrites it. |
| `set interfaces ...` unavailable in CLI | `ietf-interfaces` YANG must be present in `/usr/share/clixon/` and enabled in the `<autocli>` rules of `clixon.xml`. Both are handled by `./deploy.sh`. |
| Commit succeeds but UCI hostname unchanged | Plugin didn't load, or its XPath failed. Check `logread \| grep -i clixon`. Clixon 7.8.0 does **not** support the `namespace()` XPath function — use plain paths like `system/hostname`. |
| `ldd` on the `.so` shows unresolved `clixon_*` symbols | Expected. The plugin is dlopen'd inside `clixon_backend`, which provides those symbols via `libclixon.so`. `ldd` runs outside that context. |

## Cleanup / reset on the router

```sh
# Stop and re-enable the original packaged service
/etc/init.d/clixon stop
/etc/init.d/clixon enable

# Remove plugin files
rm /usr/lib/clixon/backend/uci_hostname.so
rm /usr/lib/clixon/clispec/system.cli
rm /usr/share/clixon/ietf-system@2014-08-06.yang
rm /usr/share/clixon/iana-crypt-hash@2014-08-06.yang

# Restore packaged config (reinstall package if you need the original)
apk add --force-reinstall clixon
```
