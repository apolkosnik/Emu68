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

## demo

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
[BOOT] Booting Emu68 runtime/ARM v7-a BigEndian
[BOOT] Boot address is ff808000
[BOOT] Build ID: 85d143d4ee6a80ad18f76b85df9c52d0984d7997
[BOOT] Disassembler set up
[BOOT] ARM stack top at 0xff9da040
[BOOT] Bootstrap ends at ffa9d000
[BOOT] ISAR=02101110, FPSID=41023075, MVFR0=10110222, MVFR1=11111111
[BOOT] Detected features: DIV BITFLD BITCNT VDIV VSQRT
[BOOT] Args=00000000,00000000,00400000,00000000
[BOOT] Local memory pool:
[BOOT]    ffa9d000 - ffff0000 (size=5582848)
[BOOT] System memory: 0x00000000-0x3f7fffff
[BOOT] Skipping kernel relocation
[BOOT] ARM Clock at 700 MHz
[BOOT] Display size is 640x480
[BOOT] Framebuffer @ 3c100000
[BOOT] Logo start coordinate: 20x95, size: 600x290
[BOOT] Initlializing Z2 RAM expansion
[BOOT]   Z2 expansion disabled
[BOOT] Loading executable from 0x01800000-0x01880000
[BOOT] Loading ROM from 0x01800000, size 524288
[BOOT] ARM32 PiStorm QEMU ROM bus-hole mode enabled for 0x00a00000-0x00b7ffff and 0x00c00000-0x00d9ffff
[BOOT] ARM32 PiStorm fake low RAM: 8 MB @ virt=3cc00000 phys=3cc00000
[BOOT] ARM32 PiStorm fake slow RAM: 2 MB @ virt=3d400000 phys=3d400000
[BOOT] ARM32 PiStorm stray-store sink: 1 MB @ virt=3d600000 phys=3d600000 (2824 sections aliased)
[BOOT] ARM32 PiStorm fake low RAM seed low0=4c4f574d
[BOOT] ROM exec-entry image detected. Reset SP=3f7ff000 PC=00f800d2
[ICache] Initializing caches
[ICache] Setting up LRU
[ICache] Setting up ICache
[ICache] Temporary code at 0xffb9e300
[ICache] ICache array at 0xffa9e2c0
[BOOT] ARM32 PiStorm move_slow_to_chip enabled
[BOOT] ARM32 PiStorm block_c0 enabled
[BOOT] ARM32 PiStorm z3_disable enabled
[BOOT] ARM32 PiStorm fake low RAM requested: 8 MB

M68K Context:
    D0 = 0x00000000    D1 = 0x00000000    D2 = 0x00000000    D3 = 0x00000000
    D4 = 0x00000000    D5 = 0x00000000    D6 = 0x00000000    D7 = 0x00000000
    A0 = 0x00000000    A1 = 0x00000000    A2 = 0x00000000    A3 = 0x00000000
    A4 = 0x00000000    A5 = 0x00000000    A6 = 0xffa9e000    A7 = 0x3f7ff000
    PC = 0x00f800d2    SR = .....    CACR=0x00000000    VBR = 0x00000000
    USP= 0x00000000    MSP= 0x00000000    ISP= 0x3f7ff000
    FP0 = 0000000000000000    FP1 = 0000000000000000    FP2 = 0000000000000000    FP3 = 0000000000000000
    FP4 = 0000000000000000    FP5 = 0000000000000000    FP6 = 0000000000000000    FP7 = 0000000000000000
    FPSR=0x00000000    FPIAR=0x00000000   FPCR=0x0000
[JIT] Let it go...


Amiga DiagROM V1.3 - 24-Aug-25  - By John (Chucky/The Gang) Hertell

- Parallel Code $ff - Start of ROM, CPU Seems somewhat alive
- Testing ROM Address-access
   OK
Testing if serial loopbackadapter is installed: <> NOT DETECTED
    Checking status of mousebuttons at power-on:
    Set all Interrupt enablebits (INTENA $dff09a) to Disabled: Done
    Set all Interrupt requestbits (INTREQ $dff09c) to Disabled: Done
    Set all DMA enablebits (DMACON $dff096) to Disabled: Done

Testing if OVL is working: OK
- Parallel Code $fe - Test UDS/LDS line
  - Test of writing word $AAAA to $400 OK
  - Test of writing word $00AA to $400 OK
  - Test of writing word $AA00 to $400 OK
  - Test of writing word $0000 to $400 OK
  - Test of writing byte (even) $AA to $400 OK
  - Test of writing byte (odd) $AA to $401 OK
- Parallel Code $fd - Start of chipmemdetection

Addr $001F0400   OK  Number of 64K blocks found: $20
Startaddr: $00000400  Endaddr: $001FFFFF
- Testing detected Chipmem for addresserrors
   - Filling memoryarea with addressdata
...............................
   - Checking block of ram that it contains the correct addressdata
...............................   CHIPMEM OK
    - Checking status of mousebuttons for different startups, if still pressed
      we assume not working and ignore those in the future.
      Green newly pressed, Yellow pressed at startup - Startupaction taken.
      Red = Pressed at both poweron and now so it is stuck and being ignored


  The following special action will be taken:
NONE
  - Fastmemcheck skipped as we found chipmem
- Parallel Code $fb - Memorydetection done

  Using $001EC876 as start of workmem (Base)

- Testing Workarea Address-access
   WORKAREA OK
- Parallel Code $fa - Starting to use detected memory

Testing if serial loopbackadapter is installed: <> NOT DETECTED
Detecting if we have a working raster: DETECTED
Detected Chipmem: 2144259696kB
Detected Motherboard Fastmem (not reliable result): 2144259696kB
Basememory address (Start of workarea): $001EC876
As a very fast test of variablearea working this SHOULD read OK:
- Parallel Code $f9 - Detected memory in use, we now have a stack etc
 - Doing Initstuff
 - Setting up Chipmemdata
   - Copy Menu Copperlist from ROM to Chipmem
   - Copy ECS TestCopperlist from ROM to Chipmem
   - Copy ECS testCopperlist2 from ROM to Chipmem
   - Fixing Bitplane Pointers etc in Menu Copperlist
   - Copy Audio Data from ROM to Chipmem
   - Do final Bitplanedata in Menu Copperlist
 - Initstuff done!

    Set Start of copper (COP1LCH $dff080): Done
    Starting Copper (COPJMP1 $dff088): Done
    Set all DMA enablebits (DMACON $dff096) to Enabled: Done
    Set Beam Conter control register to 32 (PAL) (BEAMCON0 $dff1dc): Done
    Set POTGO to all OUTPUT ($FF00) (POTGO $dff034): Done
- Parallel Code $f8 - Starting up screen, text echoed to serialport
qemu: terminating on signal 2