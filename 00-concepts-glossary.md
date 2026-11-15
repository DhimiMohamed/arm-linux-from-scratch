# 00 — Concepts & glossary

Read section 1 once before starting. Come back to section 3 whenever a step uses a term you don't know.

## 1. The project in one paragraph

You build a **tiny Linux system for an ARM processor**, using only your x86 PC.
Your PC can't run ARM code directly. So you **cross-compile** programs on the PC, put them into a
**disk image** (`arm.img`) holding a minimal Linux directory tree (the **root filesystem**),
and boot that image on an **emulated ARM board** with **QEMU**. One of the programs is your own C
application: it has **two threads**, and thread 2 waits until thread 1 has finished. The application
starts on its own when Linux boots.

---

## 2. The big picture in five diagrams

### 2.1 Host and target

```
 Your PC (x86_64, Windows)
 └── WSL2: a real Linux (Ubuntu)                ← the HOST: where you build things
      ├── cross-compiler arm-linux-gnueabi-gcc  ← runs on x86, produces ARM code
      ├── BusyBox → sh, ls, mount, init ...     ← ARM binaries
      ├── your app → threads_app                ← ARM binary
      ├── arm.img = the root filesystem         ← /bin /sbin /usr /lib /etc /dev ...
      ├── zImage + .dtb = kernel + board description
      └── QEMU = emulated ARM board             ← the TARGET: where things run
```

### 2.2 Cross-compilation

```
                 on the HOST (x86_64)                                  on the TARGET (ARM)

 threads_app.c ──► arm-linux-gnueabi-gcc ──► threads_app ──copy──► /usr/bin/threads_app
                   (an x86 program that       (ARM machine code,     runs on the Cortex-A9
                    writes ARM code)           ELF 32-bit)

 threads_app.c ──► gcc (native) ───────────► threads_app_x86 ──► runs on your PC only
```

### 2.3 What happens when you start a dynamic program

```
 you type: threads_app
      │
      ▼
 kernel reads the ELF header: "interpreter = /lib/ld-linux.so.3"  ← readelf -l threads_app | grep interpreter
      │      missing?  → "/bin/sh: threads_app: not found"  (even though the file exists!)
      ▼
 kernel starts the dynamic loader /lib/ld-linux.so.3
      │ reads the NEEDED list: libc.so.6                          ← readelf -d threads_app | grep NEEDED
      │      missing?  → "error while loading shared libraries: libc.so.6"
      ▼
 loader maps /lib/libc.so.6 into memory, connects printf, pthread_create ...
      │
      ▼
 main() runs
```

### 2.4 The boot chain: the key idea of the whole project

```
qemu-system-arm  (emulated vexpress-a9 board)
   │ loads zImage + dtb into RAM, attaches arm.img as an SD card
   ▼
Linux kernel  (zImage)
   │ finds the hardware (from the dtb), mounts /dev/mmcblk0 (= arm.img) as "/"
   ▼
/sbin/init  (BusyBox, PID 1) ── reads /etc/inittab
   ├─► ::sysinit  → /etc/init.d/rcS
   │                ├─ mounts /proc /sys /tmp (/dev)
   │                └─ runs /usr/bin/threads_app
   └─► ::askfirst → /bin/sh on the console
```

### 2.5 The console: how you talk to the board

```
 you type ──► your terminal ──► QEMU ──► board UART (PL011) ──► /dev/console ──► shell (/bin/sh)
 you read ◄── your terminal ◄── QEMU ◄── board UART (PL011) ◄── /dev/console ◄── kernel log, app output, shell
```
With `-nographic`, QEMU wires the board's serial port (UART) to your terminal. The kernel calls that
UART `ttyAMA0`, and `console=ttyAMA0` makes it the console. No screen, no keyboard: everything goes through it.

---

## 3. Glossary, grouped by topic

### A. Basic vocabulary

| Term | Meaning |
|---|---|
| **Embedded Linux** | Linux running on a device that is not a general-purpose PC: a router, a car ECU, a Raspberry Pi, a set-top box. These systems are small, use custom builds and usually run on ARM. |
| **Host** | The machine where you *develop and compile*: your PC/WSL2. |
| **Target** | The machine where the program *runs*: the ARM board, emulated here by QEMU. |
| **Architecture (ISA)** | The machine-language "dialect" a CPU understands. **x86_64** is your PC's. **ARM** is used in phones and embedded boards. An ARM binary cannot run on x86 directly. |
| **ARMv5 / ARMv7 / ARMv8** | ARM processor generations. ARMv7 (e.g. the Cortex-A9 we emulate) is 32-bit. ARMv8 adds 64-bit (`aarch64`). Code built for an older generation runs on a newer one, not the reverse. |

### B. Compiling for another CPU

| Term | Meaning |
|---|---|
| **Compilation** | Turning C source (`.c`) into machine code (an executable). |
| **Cross-compilation** | Compiling on the host (x86) to get an executable for a **different** architecture (ARM). |
| **Toolchain** | The set of tools that build programs: compiler (`gcc`), assembler, linker (`ld`), binutils (`readelf`, `objdump`, `strip`...), plus the C library for the target. |
| **Target triplet / prefix** | The tool-name prefix that says which target the tools build for: `arch-[vendor-]os-abi`. `arm-linux-gnueabi-gcc` = ARM CPU, Linux OS, GNU EABI. Another example: `aarch64-linux-gnu-gcc` for 64-bit ARM. |
| **`CROSS_COMPILE`** | The variable the kernel and BusyBox build systems use to find the cross-tools: with `CROSS_COMPILE=arm-linux-gnueabi-`, `make` calls `arm-linux-gnueabi-gcc` instead of `gcc`. |
| **`ARCH`** | The variable that tells the kernel and BusyBox build systems which architecture to configure for (`arm`). |
| **ABI** | *Application Binary Interface*: the binary-level rules, such as how function arguments are passed in registers and how floats are handled. Two binaries with different ABIs can't be mixed. |
| **EABI / gnueabi** | The *Embedded ABI* for ARM, in its GNU/Linux flavor. |
| **armel vs armhf** | `gnueabi` = **armel**: soft-float calling convention, runs on ARMv5 and newer, so it is the **most compatible**. `gnueabihf` = **armhf**: hard-float, needs ARMv7 or newer, and is faster. We use **armel**. The two are not mixable: they even use different loaders (`ld-linux.so.3` vs `ld-linux-armhf.so.3`). |
| **Sysroot** | A folder on the host that mirrors the **target's** `/lib` and `/usr/include`: ARM libraries and headers. The cross-compiler links against it. On Ubuntu it's `/usr/arm-linux-gnueabi/`. |
| **`-pthread`** | The compiler flag for POSIX threads support. It sets the right preprocessor macros **and** links the threads library. |

### C. Executables and libraries

| Term | Meaning |
|---|---|
| **ELF** | *Executable and Linkable Format*: the file format of Linux executables and libraries. You can check one with `file myprog`. |
| **Library** | Reusable compiled code, such as `printf` or `pthread_create`. |
| **Static linking** | The library code is **copied into** the executable. The file is bigger but needs nothing else at runtime. |
| **Dynamic linking / shared library (`.so`)** | The executable only **references** the library. At runtime the library file must exist on the target, usually in `/lib`. This is why we identify the shared libs and copy them into the image. |
| **`readelf -d`** | Shows the *dynamic section* of an ELF file. The `NEEDED` lines (printed as "Shared library: [...]") list the `.so` files the program requires. |
| **Dynamic loader / interpreter** (`ld-linux.so.3`) | The small program that loads shared libraries when a dynamic executable starts. Its path is written inside the ELF; see it with `readelf -l prog \| grep interpreter`. **If it is missing, you get the confusing error `not found`, even though the file exists!** `readelf -d` does *not* show it. See diagram 2.3. |
| **libc / glibc** | The C standard library (`printf`, `malloc`, `open`...). **glibc** is the GNU one. Others exist (musl, uClibc-ng) for small systems. |
| **libpthread** | The POSIX threads library. Since glibc 2.34 it is merged into `libc.so.6`, so modern programs need only libc. |
| **Strip** | Removes debug symbols from a binary to make it smaller (`arm-linux-gnueabi-strip prog`). |

### D. The Linux system on the target

| Term | Meaning |
|---|---|
| **Kernel** | The core of Linux. It talks to the hardware, manages processes and memory, and mounts filesystems. On ARM it is a single file: **`zImage`** (a compressed, self-extracting kernel image). |
| **Bootloader** | The program that loads the kernel on real hardware (U-Boot, GRUB). With QEMU we skip it: QEMU loads the kernel directly (`-kernel zImage`). |
| **Device tree (`.dts` → `.dtb`)** | A description of the board's hardware (memory, UART, SD controller, with their addresses), written as text (`.dts`) and compiled to binary (`.dtb`). ARM kernels read it at boot instead of having each board hard-coded. |
| **defconfig** | A ready-made kernel or BusyBox configuration, e.g. `vexpress_defconfig` for our board. |
| **`.config`** | The file holding the full configuration (thousands of `CONFIG_...` options) that the build uses. |
| **LTS kernel** | *Long-Term Support*: a kernel branch maintained for years (6.6 is one). |
| **Kernel command line** | Parameters given to the kernel at boot: `root=/dev/mmcblk0` (which disk holds `/`), `console=ttyAMA0` (where to print messages), `rw` (mount read-write), `rootwait` (wait for the disk to appear). |
| **Root filesystem (rootfs)** | Everything under `/`: programs, libraries and config files. Your `arm.img` *is* the rootfs. |
| **FHS** | *Filesystem Hierarchy Standard*: the conventional layout of `/bin`, `/etc`, `/lib`, `/usr`... |
| **`/bin`, `/sbin`** | Essential user commands (`sh`, `ls`, `cat`) and system commands (`init`, `mount`, `reboot`). |
| **`/usr`** (`/usr/bin`, `/usr/sbin`) | Secondary programs. **We put our app in `/usr/bin`.** |
| **`/lib`** | Shared libraries (`libc.so.6`, `ld-linux.so.3`) and kernel modules. |
| **`/etc`** | Configuration files (`inittab`, `init.d/rcS`, `hostname`). |
| **`/dev`** | **Device nodes**: special files that represent hardware (`/dev/console`, `/dev/null`, `/dev/mmcblk0`). |
| **`/proc`** | A **virtual** filesystem (procfs) created by the kernel in RAM. It holds info about processes and the system (`/proc/cpuinfo`, `/proc/<pid>/`). The rootfs only contains an **empty folder**, mounted at boot. `ps` and `top` read it. |
| **`/sys`** | Another virtual filesystem (sysfs) that shows devices and drivers. Also mounted at boot. |
| **`/tmp`, tmpfs** | Temporary files. We mount a **tmpfs** there: a filesystem that lives in RAM and is empty at each boot. |
| **Sticky bit (`chmod 1777`)** | On a shared directory like `/tmp`: everyone can create files, but only a file's owner can delete it. |
| **Device node, major/minor, `mknod`** | A device file is identified by a type (`c` = character, `b` = block) and two numbers. Example: `/dev/console` = `c 5 1`. `mknod` creates one by hand, which needs root. |
| **devtmpfs** | A kernel feature: the kernel **creates `/dev` entries automatically** for the hardware it finds. |
| **Mount** | Attaching a filesystem (a disk, an image, `proc`...) to a folder of the tree. |
| **init / PID 1** | The **first user-space program** the kernel starts (`/sbin/init`). It starts everything else. If it is missing, can't run, or exits, you get a **kernel panic**. |
| **Kernel panic** | A fatal kernel error: the system stops. Classic boot cases: *"VFS: Unable to mount root fs"* and *"No working init found"*. |
| **inittab** | BusyBox init's config file (`/etc/inittab`). It says what to run at startup (`sysinit`), what shell to offer (`askfirst`), and what to do at shutdown. |
| **rcS** | The startup **shell script** (`/etc/init.d/rcS`) run by init. It mounts `/proc`, `/sys` and `/tmp`, sets the hostname, and starts your app. |
| **Shell** | The command interpreter (`/bin/sh`): the prompt where you type commands. |
| **Shebang (`#!/bin/sh`)** | The first line of a script: it tells the kernel which interpreter runs the file. |
| **UID / ownership** | Every file belongs to a user ID. On the target, system files must belong to `root` (UID 0). |

### E. BusyBox

| Term | Meaning |
|---|---|
| **BusyBox** | "The Swiss-army knife of embedded Linux": **one** small executable that contains hundreds of classic commands (`sh`, `ls`, `mount`, `init`, `cat`, `vi`...). Each command is an **applet**. |
| **Applet symlinks / `argv[0]`** | `/bin/ls` is just a symbolic link to `/bin/busybox`. When started, BusyBox looks at `argv[0]`, the name it was called with, and acts like `ls`. `make install` creates all of these links. |

### F. The disk image

| Term | Meaning |
|---|---|
| **Disk image (`arm.img`)** | A regular file that contains a whole filesystem, byte for byte, like a virtual USB stick. QEMU presents it to the emulated board as an SD card. |
| **ext2 / ext4** | Linux filesystem formats. **ext2** is simple, has no journal, and every kernel can read it. |
| **`mke2fs` / `mkfs.ext2`** | Formats a disk or image with ext2/3/4. The option **`-d folder`** copies a folder's content into the image **without mounting it**. |
| **`debugfs`** | Lets you inspect an ext2/3/4 image without mounting it. We use it to verify `arm.img`. |
| **`e2fsck`** | Checks an ext2/3/4 filesystem for errors. `-n` = only report, change nothing. |
| **Power-of-2 size** | QEMU requires SD card images of 32M, 64M, 128M... It refuses other sizes. |

### G. QEMU (the emulator)

| Term | Meaning |
|---|---|
| **QEMU** | An open-source **emulator**. `qemu-system-arm` emulates a whole ARM computer (CPU, RAM, serial port, SD card) in software. |
| **Machine (`-M`)** | Which board to emulate. **`vexpress-a9`** = ARM Versatile Express with a Cortex-A9 (ARMv7). |
| **`-nographic`** | No graphical window: the board's **serial console** is connected to your terminal. |
| **Serial console / UART / `ttyAMA0`** | The board's serial port. Kernel messages and the shell appear there. `ttyAMA0` is Linux's name for the ARM PL011 UART. |
| **`/dev/mmcblk0`** | Linux's name for the first SD/MMC card: here, `arm.img`. |
| **qemu-user (`qemu-arm`)** | Runs a **single** ARM Linux program on your x86 PC, without emulating a board. Handy for quick tests. |
| **Exit QEMU** | Press `Ctrl-A`, then `X`. |

### H. Threads (the application)

| Term | Meaning |
|---|---|
| **Process** | A running program, with its own memory. |
| **Thread** | A "sub-task" **inside** a process. Threads share the same memory and run concurrently. |
| **POSIX threads (pthreads)** | The standard C API for threads on Linux/Unix (`#include <pthread.h>`). |
| **`pthread_create(&t, NULL, func, arg)`** | Starts a new thread that runs `func(arg)`. |
| **`pthread_join(t, &ret)`** | **Blocks** the calling thread **until thread `t` terminates**. This is how thread 2 waits for thread 1 here. A thread can be joined **only once**. |
| **Blocking** | The thread sleeps and uses no CPU until an event occurs (here: thread 1 ending). |
| **Race condition** | A bug where the result depends on which thread runs first. |
| **Mutex / condition variable / semaphore** | Other synchronization tools, used to protect shared data or to wait for an *event* rather than for the *end* of a thread. |

### I. Your Windows environment

| Term | Meaning |
|---|---|
| **WSL2** | *Windows Subsystem for Linux 2*: a real Linux kernel running in a lightweight VM inside Windows. You get a full Ubuntu without dual-boot. |
| **Distribution (distro)** | A Linux "flavor", e.g. **Ubuntu 24.04 LTS**. |
| **`docker-desktop` distro** | If Docker Desktop is installed, it adds an internal WSL distro. **Don't use it** for development: it's not a normal Linux. |
| **Linux home (`~`) vs `/mnt/c`** | Files in `~` (inside WSL's own ext4 disk) are **fast** and keep Linux permissions. `/mnt/c/...` is your Windows C: drive: **slow** for builds, and permissions and symlinks break. **Build in `~`.** |
| **`apt`** | Ubuntu's package manager (`sudo apt install <pkg>`). |
| **Environment variable, `source`** | A named value the shell passes to programs (`$ROOTFS`). `source file.sh` runs a script **in the current shell**, so the variables it sets stay defined. |
| **CRLF vs LF** | Windows text files end lines with `\r\n`, Linux with `\n`. A script saved with CRLF fails on Linux with errors mentioning `^M` or `$'\r'`. |
