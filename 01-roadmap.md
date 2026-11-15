# 01 — Roadmap

> New terms? See [00-concepts-glossary.md](00-concepts-glossary.md).
> Each step below links to its detailed guide in `steps/`.

## What you'll build

1. **`arm.img`**: an ext2 image containing a minimal Linux root filesystem (`/bin /sbin /usr /dev /lib /etc /proc /sys /tmp /root`) and an ARM app `/usr/bin/threads_app`.
2. **A Linux kernel** (`zImage` + device tree) for an emulated ARM board.
3. **The app**: two threads running two different functions, where thread 2 stays blocked until thread 1 terminates. It **starts automatically** after the kernel boots.
4. A **QEMU boot** of the whole thing.

## Requirements

| Item | Needed | Notes |
|---|---|---|
| OS | Windows 10/11 with **WSL2**, or any Linux (Ubuntu 22.04/24.04 recommended) | On native Linux, skip the WSL part of Step 1 |
| CPU / RAM | Any modern x86_64, **≥ 8 GB RAM** | More cores = faster kernel build |
| Disk | **~10 GB** free | Mostly the kernel sources and build |
| Internet | Yes | `apt` packages, BusyBox and kernel tarballs |
| Prior knowledge | Basic C and shell | Everything else is explained in the glossary |

## How the steps fit together

```
 Step 1 ─┬─► Step 2  threads_app ──┐
 tools   │                         ├─► Step 4  rootfs/ ──► Step 5  arm.img ──┐
         ├─► Step 3  BusyBox ──────┘                                         ├─► Step 7  boot in QEMU
         │                                                                   │
         └─► Step 6  zImage + dtb ───────────────────────────────────────────┘
```
The kernel (Step 6) does not depend on Steps 2–5: you can start its build early and work on the rootfs meanwhile.

| # | Step | Result | Time |
|---|---|---|---|
| 1 | [Set up WSL2 Ubuntu + tools](steps/step-1-setup-wsl.md) | Ubuntu with cross-gcc, QEMU, build tools | 15 min |
| 2 | [Write & cross-compile the two-thread app](steps/step-2-application.md) | `threads_app` (ARM ELF) + its list of shared libs | 10 min |
| 3 | [Build BusyBox](steps/step-3-busybox.md) | `/bin`, `/sbin`, `/usr` filled with ARM commands | 10 min |
| 4 | [Assemble the root filesystem](steps/step-4-rootfs.md) | `rootfs/`: dirs, libs, `/dev` nodes, init scripts, autostart | 10 min |
| 5 | [Create `arm.img`](steps/step-5-image.md) | the 64 MB ext2 image, verified | 5 min |
| 6 | [Build the Linux kernel](steps/step-6-kernel.md) | `zImage` + `vexpress-v2p-ca9.dtb` | 10–15 min |
| 7 | [Boot in QEMU](steps/step-7-boot-qemu.md) | the system boots, the app runs | 10 min |

**Total: about 1–1.5 h**, mostly waiting for builds.

## Design choices

| Choice | Why |
|---|---|
| Ubuntu's cross-toolchain **`gcc-arm-linux-gnueabi`** (GCC 13, glibc 2.39) | Maintained, installed with one `apt` command, and ships a ready ARM sysroot (`/usr/arm-linux-gnueabi`). |
| **armel** ABI (`gnueabi`) | Runs on any ARMv5+ CPU, so the same binaries work on many boards. |
| **Static BusyBox, dynamic app** | A static BusyBox always gives you a working shell, even if a library is missing. The dynamic app shows how shared libraries are found and loaded. |
| **`-pthread -Wall -Wextra -O2`** | The correct threads flag, plus warnings to catch bugs early. |
| **`readelf -d` + `readelf -l`** | `-d` lists the needed libraries, `-l` shows the dynamic loader. You need both to know what to copy. |
| **`mke2fs -d rootfs/`** | Builds the image straight from a folder: one command, reproducible, no loop mount. |
| **devtmpfs** + `console` and `null` created with `mknod` | The kernel fills `/dev` itself. The two manual nodes are a safety net for the very first moments of boot. |
| **ext2, 64 MB** | The simplest Linux filesystem. 64 MB because QEMU requires power-of-2 SD card sizes. |
| **`vexpress-a9` + Linux 6.6 LTS** | A board well supported by both QEMU and the mainline kernel, and a long-term-support kernel. |
| **Build in `~/arm-project` inside WSL** | Much faster than `/mnt/c`, and Linux permissions and symlinks (which BusyBox needs) are preserved. |

## Folder layout

The repository (what you read):
```
arm-linux-from-scratch/
├── README.md
├── 00-concepts-glossary.md
├── 01-roadmap.md
├── steps/step-1 … step-7 .md
├── files/ ← sources, copied into the build folder in Step 1
│   └── env.sh  threads_app.c  inittab  rcS
└── docs/screenshot.png
```

The build folder (where you work, inside WSL), once every step is done:
```
~/arm-project/                                   ($WORK)
├── env.sh  inittab  rcS  threads_app.c     ← Step 1: copied from the repo's files/
├── app/threads_app                         ← Step 2
├── busybox-1.36.1/                         ← Step 3
├── rootfs/                                 ← Steps 3-4 ($ROOTFS)
├── arm.img                                 ← Step 5
├── linux-6.6.58/                           ← Step 6
└── boot/zImage, boot/vexpress-v2p-ca9.dtb  ← Step 6
```
