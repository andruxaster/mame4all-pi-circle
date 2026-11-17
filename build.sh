#!/bin/sh
# Build MAME4ALL (Circle bare metal) for Raspberry Pi Zero / Zero W / Pi 1
# and assemble a ready-to-copy SD card folder in ./sd (no ROMs).
#
# Needs: arm-none-eabi-gcc 13.x (Arm GNU toolchain, with newlib), make, git,
#        texinfo (makeinfo), wget, zip. Run from the repository root.
set -e

ROOT=$(cd "$(dirname "$0")" && pwd)
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}
cd "$ROOT"

# 1. circle-stdlib (Circle + newlib), configured for Raspberry Pi 1 / Zero
git submodule update --init --recursive circle-stdlib
if [ ! -f circle-stdlib/Config.mk ]; then
	(cd circle-stdlib && ./configure -r 1 --kernel-max-size 48)
fi
make -C circle-stdlib -j"$JOBS"

# 2. MAME4ALL core + drivers as libmame.a
make -C mame4all-pi -f Makefile.circle -j"$JOBS"

# 3. kernel.img (always relinked, the kernel Makefile does not track libmame.a)
rm -f kernel/kernel.img kernel/kernel.elf
make -C kernel

# 4. Raspberry Pi firmware (downloaded by Circle's boot Makefile)
make -C circle-stdlib/libs/circle/boot firmware

# 5. SD card folder
SD="$ROOT/sd"
rm -rf "$SD"
mkdir -p "$SD/mame4all"
FW=circle-stdlib/libs/circle/boot
cp $FW/bootcode.bin $FW/start.elf $FW/fixup.dat $FW/LICENCE.broadcom "$SD/"
cp kernel/kernel.img boot/config.txt boot/cmdline.txt boot/README.txt "$SD/"
cp boot/mame.cfg mame4all-pi/cheat.dat mame4all-pi/hiscore.dat "$SD/mame4all/"
cp mame4all-pi/readme.txt "$SD/mame4all/readme_mame4all.txt"
mkdir -p "$SD/mame4all/skins" "$SD/mame4all/folders"
cp mame4all-pi/skins/*.bmp "$SD/mame4all/skins/"
cp mame4all-pi/folders/dir.txt "$SD/mame4all/folders/"
for d in roms samples artwork cfg hi inp memcard nvram snap sta frontend; do
	mkdir -p "$SD/mame4all/$d"
done

(cd "$SD" && rm -f ../mame4all_pi_sd.zip && zip -qr ../mame4all_pi_sd.zip .)
echo
echo "Done: copy the contents of ./sd (or mame4all_pi_sd.zip) to a FAT32 SD card"
echo "and put your ROM zips into /mame4all/roms."
