# Step 3: Build BusyBox (fills /bin, /sbin, /usr)

**Goal:** get hundreds of standard commands (`sh`, `ls`, `mount`, `init`...) as ARM binaries, installed into `rootfs/`.

## Why BusyBox?

A minimal Linux needs at least `/sbin/init` and a shell. Instead of cross-compiling bash, coreutils,
util-linux and so on, BusyBox provides **one binary** plus **symlinks**, one per command:

```
 /bin/sh    ──┐
 /bin/ls    ──┤
 /bin/mount ──┼── symlinks ──►  /bin/busybox   (one real program: ARM, static)
 /sbin/init ──┘                       │
                                      │ looks at argv[0] = the name it was started with
                                      ▼
                  "sh" → shell code    "ls" → ls code    "init" → init code ...
```

## 3.1 Download

```bash
source ~/arm-project/env.sh
cd $WORK
wget https://busybox.net/downloads/busybox-1.36.1.tar.bz2
tar xf busybox-1.36.1.tar.bz2 && cd busybox-1.36.1
```
(If busybox.net is down: `git clone --depth 1 -b 1_36_1 https://git.busybox.net/busybox busybox-1.36.1`)

## 3.2 Configure

```bash
make defconfig                      # default config: almost all applets enabled
# Build it STATIC: no shared-lib dependency, so the shell works even if a lib is missing
sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
# Disable 'tc': it doesn't compile with recent kernel headers (Ubuntu 24.04), and we don't need it
sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' .config
grep -E '^CONFIG_STATIC|CONFIG_TC\b' .config
```
`ARCH=arm` and `CROSS_COMPILE=arm-linux-gnueabi-` come from `env.sh`, so `make` uses the ARM compiler.
(To explore the options interactively, use `make menuconfig`: the same menu system as the kernel.)

> **Why static here but dynamic for the app?** BusyBox is the safety net: a static BusyBox always gives you
> a shell to debug with, even if `/lib` is broken. The app stays **dynamic** so you learn how shared libraries are found.

## 3.3 Build and install into rootfs

```bash
make -j$(nproc)
make install CONFIG_PREFIX=$ROOTFS
```
You may see warnings like *"Using 'getpwnam' in statically linked applications..."*. They are normal and harmless.

What `make install` creates:
```
$ROOTFS/
├── bin/
│   ├── busybox            ← the only real file
│   └── sh, ls, cat, mount ... → busybox
├── sbin/
│   └── init, reboot, poweroff ... → ../bin/busybox
├── usr/
│   ├── bin/   vi, tar, wc ... → ../../bin/busybox
│   └── sbin/  ... → ../../bin/busybox
└── linuxrc → bin/busybox  ← not needed, removed in Step 4
```

## 3.4 Verify

```bash
file $ROOTFS/bin/busybox            # ELF 32-bit ... ARM ... statically linked
ls -l $ROOTFS/bin | head            # ls -> busybox, sh -> busybox ...
ls $ROOTFS                          # bin  linuxrc  sbin  usr
ls -l $ROOTFS/sbin/init             # init -> ../bin/busybox
```

✅ **Done when:** `file` says *ARM* and *statically linked*, and `/sbin/init` points to BusyBox.

---

## What can go wrong

| If... | What you see | Why / fix |
|---|---|---|
| `env.sh` wasn't sourced | `file` says *x86-64*. At boot: `Kernel panic - not syncing: No working init found` | `make` used the host gcc. `source env.sh`, then `make clean` and rebuild |
| `CONFIG_STATIC` is not set | BusyBox is *dynamically linked*. If a lib or the loader is missing, init can't start → same kernel panic | Re-run the `sed` of 3.2, `make clean`, rebuild |
| `CONFIG_TC` is left enabled | Build error in `networking/tc.c` (`TCA_CBQ_...` undeclared) | Disable it as in 3.2 |
| You forget `CONFIG_PREFIX=$ROOTFS` | Everything is installed into `busybox-1.36.1/_install/`, and `rootfs/` stays empty | Re-run `make install CONFIG_PREFIX=$ROOTFS` |
| You install into a folder on `/mnt/c` | Symlinks fail or turn into plain files | Install inside `~` |
| `$ROOTFS` is empty (variable not set) | `make install` tries to install into **your host's** `/bin` and fails with `Permission denied` | Check `echo $ROOTFS` first, and never run `make install` with `sudo` |
