# Source this file in every new WSL terminal:   source ~/arm-project/env.sh
# Where this repository is cloned (set REPO yourself before sourcing if it is elsewhere)
export REPO="${REPO:-$HOME/arm-linux-from-scratch}"
export WORK="$HOME/arm-project"                 # Linux build folder (fast, outside the repo)
export ARCH=arm                                 # target architecture (kernel/BusyBox)
export CROSS_COMPILE=arm-linux-gnueabi-         # cross-tool prefix
export SYSROOT=/usr/arm-linux-gnueabi           # ARM libs/headers on the host
export ROOTFS="$WORK/rootfs"                    # root filesystem being built
cd "$WORK" 2>/dev/null || true
