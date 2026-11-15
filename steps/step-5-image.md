# Step 5: Create arm.img

**Goal:** turn the `rootfs/` folder into a single file, `arm.img`: a 64 MB ext2 filesystem.

```
  $ROOTFS/  (a folder on the host)                     arm.img  (one 64 MB file)
  ├── bin/ sbin/ usr/                                  ┌──────────────────────────────┐
  ├── lib/ etc/ dev/       mke2fs -t ext2 -d $ROOTFS   │ ext2 filesystem containing   │
  ├── proc/ sys/ tmp/ root/  ────────────────────────► │ the same tree, with owners,  │
  └── + owners, modes, symlinks, device nodes          │ modes, symlinks, dev nodes   │
                                                       └──────────────┬───────────────┘
                                                                      │ QEMU: -drive file=arm.img,if=sd
                                                                      ▼
                                                       the board sees an SD card: /dev/mmcblk0
                                                       the kernel mounts it as "/" (root=/dev/mmcblk0)
```

## 5.1 Build the image

```bash
source ~/arm-project/env.sh
cd $WORK
rm -f arm.img
sudo mke2fs -t ext2 -L rootfs -d $ROOTFS arm.img 64M
sudo chown $USER: arm.img
```
| Part | Meaning |
|---|---|
| `-t ext2` | Filesystem type: simple, no journal, read by every kernel |
| `-L rootfs` | Volume label (a name for the filesystem) |
| `-d $ROOTFS` | Copy the folder's contents into the image: files, symlinks, device nodes, owners, permissions |
| `arm.img 64M` | Create the file with this size. **It must be a power of 2** (32M, 64M, 128M...) or QEMU refuses the SD card |
| `sudo` | Read every file, whatever its owner and permissions |
| `chown $USER:` | Give the image back to you, so QEMU (run as you) can open it read-write |

## 5.2 Verify the image without mounting it

```bash
file arm.img                                   # Linux rev 1.0 ext2 filesystem data ... volume name "rootfs"
debugfs -R 'ls -l /' arm.img                   # bin dev etc lib proc ... with uid 0
debugfs -R 'ls -l /usr/bin/threads_app' arm.img
debugfs -R 'ls -l /dev' arm.img                # console, null
sudo e2fsck -fn arm.img                        # filesystem check: "clean", 0 errors
```

> `arm.img` is a **copy** of `rootfs/` at the moment you built it. If you change anything in `rootfs/` later,
> re-run 5.1.

✅ **Done when:** `debugfs` shows the full tree and `e2fsck` reports no errors.

---

## What can go wrong

| If... | What you see | Why / fix |
|---|---|---|
| The size isn't a power of 2 (e.g. `60M`) | QEMU: `Invalid SD card size: 60 MiB` | Use 32M, 64M, 128M... |
| The image is too small for `rootfs/` | `mke2fs`: `Could not allocate block` / `No free space in the filesystem` | Use the next power of 2 |
| You skip `chown $USER: arm.img` | QEMU: `Could not open 'arm.img': Permission denied` | The image belongs to root after `sudo mke2fs` |
| You change `rootfs/` but don't rebuild | QEMU boots the old content | Re-run 5.1 |
| Two QEMUs use the same image, or you mount it on the host while QEMU runs | Filesystem corruption: random errors, files missing at the next boot | Use one at a time. Rebuild the image, or repair it with `e2fsck -fy` |
| You kill QEMU instead of running `poweroff` | Usually fine with ext2, but unwritten data can be lost; `e2fsck` may report errors | Use `poweroff` in the shell, or rebuild the image |
