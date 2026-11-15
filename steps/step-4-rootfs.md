# Step 4: Assemble the root filesystem

**Goal:** complete `rootfs/` with the remaining directories, the app, its shared libraries, the init
configuration (with the app's autostart), and the `/dev` nodes.

## The target: what `rootfs/` looks like at the end, and where each piece comes from

```
$ROOTFS/
├── bin/                               ← 3.3 busybox: make install
│   ├── busybox                        ← the one real program (ARM, static)
│   ├── sh → busybox                   ← a symlink = one "applet"
│   └── ls cat cp mount ... → busybox  ← hundreds more symlinks
├── sbin/                              ← 3.3 busybox: make install
│   ├── init → ../bin/busybox          ← PID 1: the first program the kernel runs
│   └── reboot poweroff ... → ../bin/busybox
├── usr/                               ← 3.3 busybox: make install
│   ├── bin/                           ← 3.3 busybox (+ 4.1 mkdir, harmless if it exists)
│   │   ├── threads_app                ← 4.2 cp → strip → chmod 755
│   │   └── vi tar ... → ../../bin/busybox
│   └── sbin/                          ← 3.3 busybox: make install
├── lib/                               ← 4.1 mkdir
│   ├── ld-linux.so.3                  ← 4.3 cp -L from $SYSROOT/lib (the dynamic loader)
│   └── libc.so.6                      ← 4.3 cp -L from $SYSROOT/lib (the C library)
├── etc/                               ← 4.1 mkdir -p etc/init.d (creates etc/ too)
│   ├── inittab                        ← 4.4 cp $WORK/inittab
│   ├── hostname                       ← 4.4 echo "arm-qemu" > ...
│   └── init.d/
│       └── rcS                        ← 4.4 cp $WORK/rcS → chmod 755
├── dev/                               ← 4.1 mkdir; at boot the kernel mounts devtmpfs here
│   ├── console  (char 5,1)            ← 4.5 sudo mknod (fallback)
│   └── null     (char 1,3)            ← 4.5 sudo mknod (fallback)
├── proc/                              ← 4.1 mkdir; empty, rcS mounts procfs here at boot
├── sys/                               ← 4.1 mkdir; empty, rcS mounts sysfs here at boot
├── tmp/                               ← 4.1 mkdir → chmod 1777; rcS mounts a tmpfs here
└── root/                              ← 4.1 mkdir; home folder of the root user
```
Last: **4.5** `sudo chown -R root:root $ROOTFS`, so every file belongs to root.

## 4.1 Create the directories

```bash
source ~/arm-project/env.sh
cd $ROOTFS
mkdir -p dev lib etc/init.d proc sys tmp root usr/bin
chmod 1777 tmp                      # /tmp: writable by everyone (sticky bit)
rm -f linuxrc                       # BusyBox link we don't use
ls                                  # bin dev etc lib proc root sbin sys tmp usr
```
`/proc` and `/sys` stay **empty**: the kernel fills them at boot, when `rcS` mounts them.

## 4.2 Install the application

```bash
cp $WORK/app/threads_app $ROOTFS/usr/bin/
arm-linux-gnueabi-strip $ROOTFS/usr/bin/threads_app     # smaller: removes debug symbols
chmod 755 $ROOTFS/usr/bin/threads_app                   # rwxr-xr-x: executable by everyone
```
Use `arm-linux-gnueabi-strip`, not `strip`: the host's `strip` only understands x86 files.

## 4.3 Copy the shared libraries (found in Step 2.4)

```bash
cp -L $SYSROOT/lib/ld-linux.so.3 $SYSROOT/lib/libc.so.6 $ROOTFS/lib/
ls -l $ROOTFS/lib
```
- `cp -L` follows symlinks: if a sysroot file is a link, you copy the real file under the expected name.
- Rule: **every** `NEEDED` library **and** the interpreter must exist on the target, at the **exact path** the ELF asks for.

```
 threads_app (ELF) asks for:               must exist in rootfs/:            copied from (host):
   interpreter /lib/ld-linux.so.3   ──►     lib/ld-linux.so.3        ◄──      $SYSROOT/lib/ld-linux.so.3
   NEEDED      libc.so.6            ──►     lib/libc.so.6            ◄──      $SYSROOT/lib/libc.so.6
```

## 4.4 Init configuration and autostart

```bash
cp $WORK/inittab $ROOTFS/etc/inittab
cp $WORK/rcS     $ROOTFS/etc/init.d/rcS
chmod 755 $ROOTFS/etc/init.d/rcS
sed -i 's/\r$//' $ROOTFS/etc/inittab $ROOTFS/etc/init.d/rcS   # make sure: LF line endings
echo "arm-qemu" > $ROOTFS/etc/hostname
```

How the boot reaches your app:
```
 kernel
   │ runs /sbin/init  (PID 1)
   ▼
 BusyBox init ── reads /etc/inittab
   │
   ├─ ::sysinit:/etc/init.d/rcS          runs once; init WAITS for it to finish
   │     ├─ mount proc, sysfs, tmpfs (and devtmpfs if needed)
   │     ├─ hostname -F /etc/hostname
   │     └─ /usr/bin/threads_app         runs in the foreground until both threads end
   │
   ├─ ::askfirst:-/bin/sh                then: "Please press Enter to activate this console"
   │
   └─ ::shutdown:/bin/umount -a -r       at poweroff/reboot: unmount everything cleanly
```
- The app starts at the **end of `rcS`**. Because init waits for `sysinit`, the app's output isn't mixed with
  the shell prompt, and you still get a shell afterwards.
- Read [`files/rcS`](../files/rcS) and [`files/inittab`](../files/inittab): every line is commented.

## 4.5 Device nodes and ownership (needs sudo)

```bash
sudo mknod -m 600 $ROOTFS/dev/console c 5 1
sudo mknod -m 666 $ROOTFS/dev/null    c 1 3
sudo chown -R root:root $ROOTFS        # on the target, everything belongs to root
```
- Our kernel creates and mounts `/dev` automatically (**devtmpfs**). These two nodes are a **fallback** for
  the moments before that happens: the kernel opens `/dev/console` to give init its input and output.
- `chown root` matters: otherwise files belong to UID 1000 (your WSL user), a user that doesn't exist on the target.

## 4.6 Check the tree

```bash
sudo find $ROOTFS -maxdepth 2 -not -path "*/bin/*" -not -path "*/sbin/*" | sort
```
Compare with the tree at the top of this page.

✅ **Done when:** the tree matches, `ls -l $ROOTFS/etc/init.d/rcS` shows `-rwxr-xr-x root root`, and `$ROOTFS/lib` holds the two libraries.

---

## What can go wrong

| If... | What you see at boot | Why / fix |
|---|---|---|
| `ld-linux.so.3` is missing | `/bin/sh: threads_app: not found`, although `ls` shows the file | The kernel can't find the interpreter. Copy it (4.3) |
| `libc.so.6` is missing | `error while loading shared libraries: libc.so.6` | Copy it (4.3) |
| A library is copied as a symlink (no `-L`) | Same two errors: the link points to a path that doesn't exist on the target | Use `cp -L` |
| `/dev/console` is missing **and** devtmpfs isn't mounted | `Warning: unable to open an initial console.` then silence: no output, no shell | Create it with `mknod` (4.5) |
| `/dev/null` is missing | Commands that write to `/dev/null` create a normal file there, or fail with confusing errors | Create it with `mknod` (4.5) |
| `rcS` isn't executable | `can't run '/etc/init.d/rcS': Permission denied`. No mounts, no app, but you still get a shell | `chmod 755` |
| `rcS` has CRLF line endings | `can't run '/etc/init.d/rcS': No such file or directory` (the shell looked for `/bin/sh\r`) | `sed -i 's/\r$//'` |
| `/proc` isn't mounted | `ps`, `top`, `free` show nothing or fail; `mount` prints nothing | `rcS` must mount it |
| `/tmp` isn't `1777` | Non-root programs can't create temporary files (invisible here, since everything runs as root) | `chmod 1777 tmp` |
| Files stay owned by UID 1000 | `ls -l` on the target shows `1000 1000`. Harmless while you are root, but a future user 1000 could modify system files | `sudo chown -R root:root $ROOTFS` |
| The app never ends | Boot never reaches the shell: init is still waiting for `rcS` | Fix the app, or start it in the background (`threads_app &`) |
| You boot with `init=/usr/bin/threads_app` | When the app exits: `Kernel panic - not syncing: Attempted to kill init!` | PID 1 must never exit. Start the app from `rcS` |
| You edit `rootfs/` after Step 5 | Nothing changes in QEMU | Rebuild `arm.img` (Step 5) |
