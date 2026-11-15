# Step 7: Boot in QEMU

**Goal:** boot the kernel with `arm.img` as root filesystem, watch the app start on its own, then use the shell.

## 7.1 The emulated board

```
 qemu-system-arm  (a normal Linux program, running in WSL)
 ┌──────────────────────────────────────────────────────────────┐
 │ emulated board: vexpress-a9                                  │
 │                                                              │
 │   CPU: Cortex-A9      RAM: 512 MB (-m 512M)                  │
 │   kernel: -kernel zImage   hardware description: -dtb        │
 │                                                              │
 │   SD controller (PL180) ◄── -drive file=arm.img,if=sd        │
 │        Linux sees /dev/mmcblk0 → mounted as / (root=)        │
 │                                                              │
 │   UART0 (PL011)  ──  Linux: ttyAMA0 = console (console=)     │
 └────────┬───────────────────────────────▲─────────────────────┘
          │ kernel log, app output,       │ your keystrokes
          │ shell prompt                  │
          ▼                               │
 ┌──────────────────────────────────────────────────────────────┐
 │ your terminal (-nographic wires the board's UART to it)      │
 └──────────────────────────────────────────────────────────────┘
```

## 7.2 Boot

```bash
source ~/arm-project/env.sh
cd $WORK
qemu-system-arm -M vexpress-a9 -m 512M -nographic \
  -kernel boot/zImage \
  -dtb    boot/vexpress-v2p-ca9.dtb \
  -drive  file=arm.img,if=sd,format=raw \
  -append "console=ttyAMA0 root=/dev/mmcblk0 rw rootwait"
```
| Part | Meaning |
|---|---|
| `-M vexpress-a9 -m 512M` | Emulate a Versatile Express Cortex-A9 board with 512 MB RAM |
| `-nographic` | Serial console in your terminal (no window) |
| `-kernel`, `-dtb` | The kernel and the hardware description (QEMU acts as the bootloader) |
| `-drive ... if=sd,format=raw` | Insert `arm.img` as the board's SD card, which Linux sees as `/dev/mmcblk0` |
| `console=ttyAMA0` | Kernel messages go to the serial port, i.e. your terminal |
| `root=/dev/mmcblk0 rw rootwait` | Mount the SD card as `/`, read-write, and wait for it to appear |

## 7.3 What you should see

```
Booting Linux on physical CPU 0x0
...
VFS: Mounted root (ext2 filesystem) on device 179:0.
devtmpfs: mounted
...
Run /sbin/init as init process

*** System booted - starting threads_app ***
=== threads_app: 2-thread demo (pid 7x) ===
[thread 1] working... step 1/5
[thread 2] started, BLOCKED until thread 1 terminates...
[thread 1] working... step 2/5
...
[thread 1] finished -> terminating
[thread 2] thread 1 has terminated -> thread 2 unblocked
[thread 2] working... step 1/3
...
=== threads_app: all threads finished ===
*** threads_app finished ***

Please press Enter to activate this console.
```
Press **Enter** to get a shell, then explore:
```sh
/ # ls /
/ # threads_app              # run the app again by hand
/ # cat /proc/cpuinfo        # proves it's an ARM CPU
/ # mount                    # what rcS mounted
/ # ps                       # init is PID 1
/ # poweroff                 # clean shutdown (or Ctrl-A then X to quit QEMU)
```

## 7.4 Screenshot (for the README)

Make the terminal window large and capture the autostart output, from `*** System booted` to `*** threads_app finished ***`.
Windows: **Win + Shift + S**, then save it as `docs/screenshot.png` in the repo.
(A repo inside WSL is reachable from Windows Explorer at `\\wsl.localhost\Ubuntu-24.04\home\<user>\arm-linux-from-scratch`.)

## 7.5 Keep the build outputs (optional)

```bash
mkdir -p "$REPO/output"
cp arm.img boot/zImage boot/vexpress-v2p-ca9.dtb "$REPO/output/"
```
`output/` is in `.gitignore`: binary images don't belong in git history. To share them, attach them to a
**GitHub Release** instead. With these three files, anyone can boot the system using the command from 7.2.

✅ **Done:** the system boots, the app runs automatically, and you get a shell.

---

## What can go wrong: find the stage where the boot stopped

The **last message** you see tells you which stage failed:
```
 1  QEMU starts the board               (no output yet)
 2  the kernel starts                   "Booting Linux on physical CPU 0x0"
 3  the SD card is detected             "mmcblk0: mmc0:... 64.0 MiB"
 4  the root filesystem is mounted      "VFS: Mounted root (ext2 filesystem)"
 5  /dev is mounted                     "devtmpfs: mounted"
 6  the kernel starts init (PID 1)      "Run /sbin/init as init process"
 7  init runs rcS                       "*** System booted - starting threads_app ***"
 8  rcS runs the app                    "=== threads_app: 2-thread demo ..."
 9  init offers a shell                 "Please press Enter to activate this console."
```

| Stage | Symptom | Cause → fix |
|---|---|---|
| 1 | `Invalid SD card size` | Image is not a power of 2 → rebuild with `64M` (Step 5) |
| 1 | `Could not open 'arm.img': Permission denied` | The image belongs to root → `sudo chown $USER: arm.img` |
| 1–2 | Nothing at all after QEMU starts | Missing or wrong `-dtb`, or wrong `console=` → check both against 7.2 |
| 3–4 | `VFS: Unable to mount root fs on unknown-block(0,0)` | No SD card seen: kernel lacks `MMC_ARMMMCI`, or `-drive ... if=sd` is missing (Step 6.2) |
| 4 | `VFS: Unable to mount root fs on unknown-block(179,0)` | Card seen but unreadable: kernel lacks ext2/ext4, or the image is broken (Steps 5, 6.2) |
| 4 | `VFS: Cannot open root device "..."` | Typo in `root=` → `root=/dev/mmcblk0` |
| 6 | `Warning: unable to open an initial console.` | No `/dev/console` and no devtmpfs (Step 4.5, Step 6.2) |
| 6 | `Kernel panic - not syncing: No working init found` | `/sbin/init` missing, or BusyBox is not ARM / not static (Step 3) |
| 7 | `can't run '/etc/init.d/rcS': Permission denied` | `chmod 755` (Step 4.4), then rebuild the image |
| 7 | `can't run '/etc/init.d/rcS': No such file or directory` | rcS missing, or has CRLF line endings → `sed -i 's/\r$//'` (Step 4.4) |
| 8 | `/bin/sh: threads_app: not found` (the file exists!) | **Missing dynamic loader** `/lib/ld-linux.so.3` (Step 4.3) |
| 8 | `error while loading shared libraries: libc.so.6` | libc not copied (Step 4.3) |
| 8 | Strange `syntax error` or `can't execute` when running the app | The app was compiled for x86 → use `arm-linux-gnueabi-gcc` (Step 2.3) |
| 9 | The prompt never appears | The app never ends, and init waits for `rcS` (Step 4.4) |
| any | Your changes don't show up | You edited `rootfs/` but didn't rebuild `arm.img` (Step 5.1) |
