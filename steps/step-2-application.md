# Step 2: Write and cross-compile the two-thread application

**Goal:** an **ARM** executable `threads_app` with two threads running two different functions,
where **thread 2 stays blocked until thread 1 terminates**.

## 2.1 How the program works

Source: [`files/threads_app.c`](../files/threads_app.c) (already copied to `~/arm-project`).

Who creates and who waits for whom:
```
 main
  ├── pthread_create ──► thread 1: task1()   5 steps, 1 s each, then returns
  ├── pthread_create ──► thread 2: task2()   pthread_join(thread 1) ── blocked until thread 1 returns
  │                                           then 3 steps, then returns
  └── pthread_join(thread 2) ── blocked until thread 2 returns, then prints "all threads finished"
```

The same thing over time:
```
 t (s)      0    1    2    3    4    5    6    7    8
            ├────┼────┼────┼────┼────┼────┼────┼────┤
 thread 1   [s1 ][s2 ][s3 ][s4 ][s5 ]X
 thread 2   join(T1) . . BLOCKED . . [s1 ][s2 ][s3 ]X
 main       join(T2) . . . . . . . . . . . . . . . . ►

 [sN ] = one step (prints a line, then sleep 1 s)      X = the thread returns (terminates)
 . . . = blocked in pthread_join: asleep, using no CPU  ► = main prints "all threads finished"
```

- **Two distinct functions:** `task1` and `task2`.
- **Thread 2 blocked until thread 1 ends:** inside `task2`, `pthread_join(thread1, NULL)` puts thread 2
  to sleep until thread 1 returns.
- `main` joins **only thread 2**, because a thread can be joined only once and thread 2 already joins thread 1.
- `thread1` is a global variable, written by `pthread_create` **before** thread 2 exists, so thread 2 always reads a valid value.
- `setvbuf(stdout, NULL, _IONBF, 0)` makes every `printf` appear immediately, in order.
- Error codes of `pthread_*` are checked. These functions return an error number; they don't set `errno`.

## 2.2 Optional: test on the host first (x86, native)

Testing on the PC first is good practice: debug the logic quickly, then cross-compile.
```bash
cd $WORK && mkdir -p app && cp threads_app.c app/ && cd app
gcc -Wall -Wextra -O2 -pthread -o threads_app_x86 threads_app.c && ./threads_app_x86
```
Check that the `[thread 2] ... unblocked` line comes **after** `[thread 1] finished`.

## 2.3 Cross-compile for ARM

```
                 on the HOST (x86_64)                                  on the TARGET (ARM)

 threads_app.c ──► arm-linux-gnueabi-gcc ──► threads_app ──copy──► /usr/bin/threads_app  (Step 4)
                   (an x86 program that       (ARM machine code,
                    writes ARM code)           ELF 32-bit)
```

```bash
cd $WORK/app
arm-linux-gnueabi-gcc -Wall -Wextra -O2 -pthread -o threads_app threads_app.c
file threads_app
```
Expected: `ELF 32-bit LSB executable, ARM, EABI5 ..., dynamically linked, interpreter /lib/ld-linux.so.3 ...`

| Flag | Meaning |
|---|---|
| `-Wall -Wextra` | Turn on warnings |
| `-O2` | Optimize |
| `-pthread` | Enable POSIX threads (macros + library) |
| `-o threads_app` | Name of the output file |

## 2.4 Find which shared libraries it needs

A dynamic program needs two kinds of files on the target. Two different commands show them:
```bash
arm-linux-gnueabi-readelf -d threads_app | grep NEEDED          # the libraries
#  -> 0x00000001 (NEEDED)  Shared library: [libc.so.6]
arm-linux-gnueabi-readelf -l threads_app | grep interpreter     # the loader (easy to forget!)
#  -> [Requesting program interpreter: /lib/ld-linux.so.3]
```

What happens on the target when the program starts:
```
 you type: threads_app
      │
      ▼
 kernel reads the ELF header: "interpreter = /lib/ld-linux.so.3"
      │      missing?  → "/bin/sh: threads_app: not found"   (even though the file exists!)
      ▼
 kernel starts the dynamic loader /lib/ld-linux.so.3
      │ reads the NEEDED list: libc.so.6
      │      missing?  → "error while loading shared libraries: libc.so.6"
      ▼
 loader maps /lib/libc.so.6 into memory, connects printf, pthread_create ...
      │
      ▼
 main() runs
```
So the target needs **`/lib/ld-linux.so.3`** and **`/lib/libc.so.6`**. You'll copy them from the sysroot
`/usr/arm-linux-gnueabi/lib/` in Step 4. There is no `libpthread.so.0`: since glibc 2.34 it is part of libc.

## 2.5 Optional: run the ARM binary on x86 right now

`qemu-user` runs a single ARM program on your PC. `-L` tells it where to find the ARM loader and libc:
```bash
sudo apt install -y qemu-user
qemu-arm -L $SYSROOT ./threads_app
```

✅ **Done when:** `file` says *ARM* and you know the two files to copy (`ld-linux.so.3`, `libc.so.6`).

---

## What can go wrong

| If... | What you see | Why / fix |
|---|---|---|
| You compile with `gcc` instead of `arm-linux-gnueabi-gcc` | `file` says *x86-64*. On the target: `can't execute`, or a strange `syntax error` | The board can't run x86 code. Always check with `file` |
| You only run `readelf -d` | The app is "not found" on the target although it is in `/usr/bin` | The loader is listed only by `readelf -l` |
| You mix armel and armhf (`gnueabihf`) | The ELF asks for `/lib/ld-linux-armhf.so.3`, which isn't in the image → "not found" | Use one toolchain and its own sysroot for everything |
| `main` **also** joins thread 1 | Undefined behavior: random crash, hang, or error code | Join each thread exactly once |
| Thread 2 is created before thread 1 | Thread 2 may read `thread1` before it is set → joins garbage | Create thread 1 first (as in the code) |
| No `setvbuf` and output goes to a pipe or file | Lines appear late, in chunks, out of order between threads | `stdout` is fully buffered when it's not a terminal |
| `pthread_*` return values are ignored | A failure goes unnoticed and the app misbehaves silently | They return an error number: check it, print it with `strerror` |
