# Running DiagROM on Emu68 under QEMU (ARM32 / PiStorm ROM variant)

This document explains how to boot DiagROM through the Emu68 ARM32 PiStorm
build under `qemu-system-arm -M raspi2b`, both headlessly (for automated
testing) and with a visible GTK window (for interactive debugging).

## Prerequisites

* `qemu-system-arm`, `dtc`, `arm-none-eabi-gcc`, `arm-none-eabi-objcopy`
* A built ARM32 PiStorm ROM variant:
  `build-arm32-pistorm-rom/Emu68.img`
* A DiagROM image at `install/diag.rom` (512 KB)

## Required bootargs

The PiStorm ARM32 runtime needs a specific set of flags to boot DiagROM
cleanly under QEMU:

```
console=ttyAMA0 skip_reloc pistorm=qemu_rom_bushole,move_slow_to_chip,block_c0,z3_disable fake_lowram=8
```

Notes:

* `skip_reloc` keeps the kernel in place so the fake-low-RAM / stray-store
  sink MMU setup survives.
* `pistorm=…` is a comma-joined list parsed by `start_rpi.c`. It enables the
  QEMU ROM bus-hole, routes slow RAM into chip RAM, blocks the `$c0xxxx`
  overlay, and disables Zorro III.
* **`fake_lowram=8` must be a separate bootargs word**, not folded into the
  comma-joined `pistorm=` value. The parser's `find_token()` looks for
  `fake_lowram=` as its own whitespace-delimited token.

Without `fake_lowram=8` you will not get the 8 MB fake low RAM window plus
the 2 MB slow-RAM window and the 1 MB stray-store sink. DiagROM's chip-RAM
walk at `pc=0xf80252` relies on the sink to catch stray stores to unmapped
sections — without it the ARM side takes a data abort and spins.

## Headless run (via the smoke-test wrapper)

The smoke-test script is the easiest way to run from the command line. It
builds the bootstub and DTB on the fly and pipes serial to stdout.

```bash
EMU68_QEMU_BOOTARGS="console=ttyAMA0 skip_reloc pistorm=qemu_rom_bushole,move_slow_to_chip,block_c0,z3_disable fake_lowram=8" \
EMU68_QEMU_INITRD=/home/adam/emu68/Emu68/install/diag.rom \
EMU68_QEMU_TIMEOUT=60 \
EMU68_QEMU_EXPECT="nonexistent-marker" \
/home/adam/emu68/Emu68/scripts/run-qemu-raspi32-smoke.sh \
  /home/adam/emu68/Emu68/build-arm32-pistorm-rom/Emu68.img
```

Key points:

* The first positional argument must be the **PiStorm ROM variant** image.
  The default `build-arm32/Emu68.img` is a HUNK-payload build and does not
  boot the ROM path.
* `EMU68_QEMU_EXPECT="nonexistent-marker"` makes the script run until the
  `EMU68_QEMU_TIMEOUT` deadline fires. The script exits with status 1 in
  that case — expected, it just means "timed out waiting for a marker that
  will never appear."
* Bump `EMU68_QEMU_TIMEOUT` to give DiagROM more time. 60–120 seconds is
  usually plenty to watch it go through the menu.
* Add `EMU68_QEMU_LIVE_SERIAL=1` to stream serial output live instead of
  dumping it all at the end.

## Interactive run with a GTK window

The smoke-test script sends serial to a pipe and wraps QEMU in `timeout`,
which suppresses the GTK window on some setups. To see DiagROM's
framebuffer, build the DTB and bootstub manually and invoke QEMU directly.

Paste the whole block into one shell:

```bash
TMP=/tmp/emu68-direct
mkdir -p "$TMP"

INITRD=/home/adam/emu68/Emu68/install/diag.rom
INITRD_SIZE=$(stat -c%s "$INITRD")
INITRD_END=$(printf '0x%08x' $((0x01800000 + INITRD_SIZE)))

sed -e 's|@EMU68_BOOTARGS@|console=ttyAMA0 skip_reloc pistorm=qemu_rom_bushole,move_slow_to_chip,block_c0,z3_disable fake_lowram=8|' \
    -e "s|@EMU68_INITRD_END@|${INITRD_END}|" \
    /home/adam/emu68/Emu68/scripts/qemu-raspi2-arm32-smoke.dts \
    > "$TMP/raspi2-smoke.dts"

dtc -q -I dts -O dtb -o "$TMP/raspi2-smoke.dtb" "$TMP/raspi2-smoke.dts"

arm-none-eabi-gcc -nostdlib -march=armv7-a -marm -Wl,-Ttext=0x1000 \
    -o "$TMP/bootstub.elf" \
    /home/adam/emu68/Emu68/scripts/qemu-raspi2-arm32-bootstub.S
arm-none-eabi-objcopy -O binary "$TMP/bootstub.elf" "$TMP/bootstub.bin"

qemu-system-arm -M raspi2b \
    -device "loader,file=$TMP/bootstub.bin,addr=0x00001000,force-raw=on" \
    -device loader,addr=0x00001000,cpu-num=0 \
    -device "loader,file=/home/adam/emu68/Emu68/build-arm32-pistorm-rom/Emu68.img,addr=0x00008000,force-raw=on" \
    -device "loader,file=$TMP/raspi2-smoke.dtb,addr=0x00400000,force-raw=on" \
    -device "loader,file=$INITRD,addr=0x01800000,force-raw=on" \
    -display gtk \
    -serial stdio \
    -monitor none
```

Stop with Ctrl-C in the terminal. Serial output appears in the terminal;
the GTK window shows DiagROM's framebuffer.

Notes:

* Use `-display sdl` if GTK is not available. Check what your QEMU
  supports with `qemu-system-arm -display help`.
* The DTB template needs both `@EMU68_BOOTARGS@` and `@EMU68_INITRD_END@`
  substituted. `@EMU68_INITRD_END@` is the m68k end address of the ROM
  image (0x01800000 + file size). For the 512 KB DiagROM this is
  `0x01880000`.
* On a remote shell you need `DISPLAY` / `WAYLAND_DISPLAY` set (or X11
  forwarding) before the window can render.
* If the window opens but stays blank, QEMU's raspi2b framebuffer model
  may not be routing Emu68's mailbox-allocated framebuffer to the GTK
  surface. The serial path (DiagROM banner, CHIPMEM OK, menu) still works
  regardless.

## Related bits in the source

* Bootargs parser: `src/raspi/start_rpi.c` (search for `fake_lowram=`,
  `move_slow_to_chip`, `block_c0`, `z3_disable`).
* Fake low RAM / slow RAM / stray-store sink setup:
  `src/raspi/start_rpi.c` → `pistorm_init_fake_lowram()`.
* DTB template: `scripts/qemu-raspi2-arm32-smoke.dts`.
* Smoke-test wrapper: `scripts/run-qemu-raspi32-smoke.sh`.
