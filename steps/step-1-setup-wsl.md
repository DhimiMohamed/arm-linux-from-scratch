# Step 1: Set up WSL2 Ubuntu and the tools

**Goal:** get a real Linux (Ubuntu 24.04) inside Windows, with the ARM cross-compiler, QEMU and the kernel build tools.

> On a native Linux machine, skip 1.1 and start at 1.2.

## Where things live

```
 Windows
 ├── C:\Users\<you>\...                   ← seen from Ubuntu as /mnt/c/Users/<you>/... (slow, no Linux permissions)
 └── WSL2 (a small virtual machine)
     └── Ubuntu 24.04
         ├── /mnt/c/                      ← your C: drive
         └── /home/<user>/  = ~           ← fast Linux disk (ext4): build here
             ├── arm-linux-from-scratch/  ← this repo, if you clone it inside WSL
             └── arm-project/             ← the build folder ($WORK)
```

## 1.1 Install Ubuntu (PowerShell, on Windows)

```powershell
wsl --update
wsl --install -d Ubuntu-24.04
```
A window opens and asks for a **Linux username and password**. Choose simple ones and remember the
password: `sudo` will ask for it.

Make Ubuntu the default distro (if Docker Desktop is installed, `wsl` may otherwise open its internal distro):
```powershell
wsl --set-default Ubuntu-24.04
wsl -l -v          # Ubuntu-24.04 should show VERSION 2 and a *
```
Open Ubuntu from the Start menu, or type `wsl` in PowerShell / Windows Terminal.

## 1.2 Install the packages (inside Ubuntu)

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y \
  build-essential bc bison flex libssl-dev libncurses-dev \
  wget xz-utils bzip2 file e2fsprogs \
  gcc-arm-linux-gnueabi \
  qemu-system-arm
```

| Package | Why |
|---|---|
| `build-essential` | Native gcc and make, used by the kernel/BusyBox build systems themselves |
| `bc bison flex libssl-dev libncurses-dev` | Required to configure and build the Linux kernel (`ncurses` = the `menuconfig` UI) |
| `wget xz-utils bzip2` | Download and extract source tarballs |
| `file e2fsprogs` | `file` identifies binaries. `e2fsprogs` provides `mke2fs` / `debugfs` / `e2fsck` for the image |
| **`gcc-arm-linux-gnueabi`** | **The ARM cross-toolchain**: gcc, binutils, and glibc for ARM in `/usr/arm-linux-gnueabi` |
| **`qemu-system-arm`** | **The ARM board emulator** |

What the cross-toolchain package puts on your disk:
```
/usr/bin/arm-linux-gnueabi-gcc                            ← the cross-compiler (an x86 program)
/usr/bin/arm-linux-gnueabi-readelf, -strip, -objdump ...  ← binutils for ARM files
/usr/arm-linux-gnueabi/                                   ← the sysroot ($SYSROOT)
├── include/                                              ← ARM C headers (stdio.h, pthread.h ...)
└── lib/
    ├── ld-linux.so.3                                     ← dynamic loader, to copy to the target
    └── libc.so.6                                         ← C library, to copy to the target
```

## 1.3 Verify

```bash
arm-linux-gnueabi-gcc --version     # GCC 13.x
qemu-system-arm --version           # QEMU 8.x
qemu-system-arm -M help | grep vexpress-a9
ls /usr/arm-linux-gnueabi/lib/ | grep -E 'ld-linux|libc.so'   # the target libs (sysroot)
```

## 1.4 Create the workspace (in Linux, NOT on /mnt/c)

### The idea: two folders

| Folder | What it is | Why |
|---|---|---|
| **The repo** (`arm-linux-from-scratch/`) | This guide + the source files in `files/` | You read and edit it |
| **The build folder** (`~/arm-project/`) | Where everything is compiled | Must be **inside Linux** (`~`): builds on `/mnt/c` are slow and break BusyBox symlinks |

So: **(1)** tell the shell where the repo is, **(2)** create the build folder, **(3)** copy `files/` into it.

```
 $REPO/files/                         ~/arm-project/   ($WORK)
 ├── env.sh            ──── cp ────►  ├── env.sh
 ├── threads_app.c                    ├── threads_app.c
 ├── inittab                          ├── inittab
 └── rcS                              └── rcS            (then: sed removes CRLF)
```

### 1.4.1 Tell the shell where the repo is (`REPO`)

Pick **one** case:

**Case A: you downloaded or cloned the repo on Windows.** WSL sees your `C:` drive as `/mnt/c`, with `\` replaced by `/`.
Example: `C:\Users\alice\Documents\arm-linux-from-scratch` becomes `/mnt/c/Users/alice/Documents/arm-linux-from-scratch`.
```bash
export REPO=/mnt/c/Users/<your-windows-user>/<path>/arm-linux-from-scratch    # <-- adapt this line
ls $REPO/files          # must show: env.sh  inittab  rcS  threads_app.c
```

**Case B: clone it directly inside WSL** (`git` is preinstalled on Ubuntu):
```bash
git clone https://github.com/DhimiMohamed/arm-linux-from-scratch.git ~/arm-linux-from-scratch
export REPO=~/arm-linux-from-scratch
ls $REPO/files          # must show: env.sh  inittab  rcS  threads_app.c
```

> If `ls $REPO/files` says *No such file or directory*, the path is wrong. Fix it before continuing.
> If the path contains spaces, put it in quotes: `export REPO="/mnt/c/Users/alice/My Documents/arm-linux-from-scratch"`.

### 1.4.2 Create the build folder and copy the files

```bash
mkdir -p ~/arm-project                  # create the build folder (-p: no error if it exists)
cp $REPO/files/* ~/arm-project/         # copy the sources into it
sed -i 's/\r$//' ~/arm-project/*        # convert Windows line endings (CRLF) to Linux (LF)
```

### 1.4.3 Load the environment variables

```bash
source ~/arm-project/env.sh             # defines the variables below and goes to ~/arm-project
echo $CROSS_COMPILE                     # -> arm-linux-gnueabi-
ls                                      # -> env.sh  inittab  rcS  threads_app.c
```

| Variable | Value | Used for |
|---|---|---|
| `WORK` | `~/arm-project` | the build folder |
| `ROOTFS` | `~/arm-project/rootfs` | the root filesystem being assembled (Steps 3–5) |
| `SYSROOT` | `/usr/arm-linux-gnueabi` | where the ARM libraries are (Step 4) |
| `ARCH` | `arm` | tells the kernel and BusyBox builds to target ARM |
| `CROSS_COMPILE` | `arm-linux-gnueabi-` | makes `make` call `arm-linux-gnueabi-gcc` instead of `gcc` |
| `REPO` | your repo path | where the guide and `files/` are |

Variables disappear when you close the terminal. To load them automatically in every new terminal, run this **once**
(use the same `REPO` path as in 1.4.1):
```bash
echo "export REPO=$REPO" >> ~/.bashrc
echo 'source ~/arm-project/env.sh' >> ~/.bashrc
```

> 💡 Optional: in VS Code, install the **WSL** extension, then run `code .` in `~/arm-project` to edit Linux files comfortably.

✅ **Done when:** the verification commands in 1.3 print versions/files, `echo $CROSS_COMPILE` prints
`arm-linux-gnueabi-`, and `ls ~/arm-project` shows `env.sh inittab rcS threads_app.c`.

---

## What can go wrong

| If... | What you see | Fix |
|---|---|---|
| The default distro is Docker's, not Ubuntu | `wsl` opens a strange shell, `apt: not found` | `wsl --set-default Ubuntu-24.04` |
| Ubuntu runs as WSL **1** | `wsl -l -v` shows VERSION 1; builds are very slow | `wsl --set-version Ubuntu-24.04 2` |
| You build under `/mnt/c` | Builds are 10× slower; BusyBox `make install` fails to create symlinks or creates broken ones | Build in `~/arm-project` |
| You forget `source env.sh` in a new terminal | `$WORK` and `$ROOTFS` are empty, and **`make` silently builds for x86** instead of ARM. You find out much later, at boot | Add it to `~/.bashrc` (1.4.3); check with `echo $CROSS_COMPILE` |
| The files keep Windows line endings (CRLF) | `$'\r': command not found` when sourcing `env.sh` | `sed -i 's/\r$//' ~/arm-project/*` |
| `REPO` points to the wrong folder | `cp: cannot stat '.../files/*'` | Check with `ls $REPO/files` |
| A package is missing | Later: `bison: not found`, `flex: not found`, `openssl/opensslv.h: No such file` | Re-run the `apt install` of 1.2 |
