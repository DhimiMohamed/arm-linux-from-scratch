# Step 6: Build the Linux kernel for the emulated board

**Goal:** get `zImage` (the kernel) and `vexpress-v2p-ca9.dtb` (the board description) for QEMU's `vexpress-a9` board.

```
 linux-6.6.x/  (source)
      │ make vexpress_defconfig         → .config: a ready-made configuration for this board
      │ ./scripts/config -e ...         → force the options we rely on
      │ make olddefconfig               → resolve the dependencies of those options
      │ make zImage dtbs                → compile, with ARCH=arm and CROSS_COMPILE=arm-linux-gnueabi-
      ▼
 arch/arm/boot/zImage                         the kernel (compressed, self-extracting)
 arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb   the board description (device tree)
```

## 6.1 Download Linux 6.6 LTS

```bash
source ~/arm-project/env.sh
cd $WORK
KVER=6.6.58        # any 6.6.x works; the latest is listed on https://kernel.org
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$KVER.tar.xz
tar xf linux-$KVER.tar.xz && cd linux-$KVER
```

## 6.2 Configure

```bash
make vexpress_defconfig                     # ready-made config for the Versatile Express board
# Make sure the options we rely on are built in:
./scripts/config -e DEVTMPFS -e DEVTMPFS_MOUNT -e EXT2_FS -e EXT4_FS -e MMC_ARMMMCI
make olddefconfig                           # resolve dependencies of the changed options
grep -E 'CONFIG_(DEVTMPFS|DEVTMPFS_MOUNT|EXT4_FS|MMC_ARMMMCI)=' .config
```
| Option | Why |
|---|---|
| `DEVTMPFS`, `DEVTMPFS_MOUNT` | The kernel creates and mounts `/dev` automatically |
| `EXT2_FS` / `EXT4_FS` | To read our ext2 `arm.img` (the ext4 driver also handles ext2) |
| `MMC_ARMMMCI` | The driver for the board's SD card controller (ARM PL180), where `arm.img` is attached |

`ARCH=arm` and `CROSS_COMPILE` come from `env.sh`. Without them, `make` would configure an x86 kernel.

Which kernel pieces each part of the boot depends on:
```
 arm.img on the SD card ──► MMC_ARMMMCI driver ──► /dev/mmcblk0 ──► EXT2/EXT4 driver ──► mounted as "/"
                                                                                           │
 DEVTMPFS + DEVTMPFS_MOUNT ──► /dev filled and mounted ──► /dev/console ──► init has a console
```

## 6.3 Build

```bash
make -j$(nproc) zImage dtbs                 # ~5-15 min, depending on your CPU
ls -lh arch/arm/boot/zImage
ls arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb
```
Since Linux 6.5, ARM device trees are grouped in vendor sub-folders, hence `dts/arm/`.

What the device tree tells the kernel (a simplified extract):
```
 vexpress-v2p-ca9.dtb
 ├── cpus:    Cortex-A9
 ├── memory   @ 0x60000000        ← where the RAM is
 ├── uart0    PL011 @ 0x10009000  ← becomes ttyAMA0, our console
 └── mmci     PL180 @ 0x10005000  ← the SD controller, gives /dev/mmcblk0
```
Read the source yourself: `arch/arm/boot/dts/arm/vexpress-v2m.dtsi` and `vexpress-v2p-ca9.dts`.

## 6.4 Keep the outputs at hand

```bash
mkdir -p $WORK/boot
cp arch/arm/boot/zImage arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb $WORK/boot/
```

✅ **Done when:** `$WORK/boot/` contains `zImage` and `vexpress-v2p-ca9.dtb`.

---

## What can go wrong

| If... | What you see | Why / fix |
|---|---|---|
| `env.sh` wasn't sourced | `Can't find default configuration "arch/x86/configs/vexpress_defconfig"` | `ARCH` is not set. `source env.sh` |
| A build tool is missing | `bison: not found`, `flex: not found`, `bc: not found`, `openssl/opensslv.h: No such file` | Install the packages of Step 1.2 |
| `MMC_ARMMMCI` is off | Boot: `Kernel panic - not syncing: VFS: Unable to mount root fs on unknown-block(0,0)` | No SD driver, so no `/dev/mmcblk0` |
| `EXT2_FS` and `EXT4_FS` are off | Boot: `... Unable to mount root fs on unknown-block(179,0)` | The disk is found (179 = MMC) but its filesystem can't be read |
| `DEVTMPFS_MOUNT` is off | `/dev` only holds the two nodes from Step 4, until `rcS` mounts devtmpfs (our `rcS` does) | That's why both the kernel option and the `rcS` line exist |
| You skip `make olddefconfig` | The build stops to ask questions, or an option you forced is silently dropped | Run it after every `scripts/config` |
| You use a `.dtb` for another board | QEMU starts, then nothing at all is printed | Use `vexpress-v2p-ca9.dtb` with `-M vexpress-a9` |
| You look for `dts/vexpress-v2p-ca9.dtb` | `No such file or directory` | Since 6.5 it's in `dts/arm/` |
| WSL runs out of memory | `Killed signal terminated program cc1` | Use fewer jobs: `make -j4` |
