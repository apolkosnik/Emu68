# Emu68
M68K emulation for ARM



# Building instructions

In order to build Emu68 several tools are necessary. The first one is of course git, which will be used to clone the repository. Further, cmake will be used to configure the build. Firmware, if required, will be downloaded by cmake during project configure phase, here, either curl or wget will be used. Finally, an AArch64 or AArch32 toolchain will be necessary.

## Building on Ubuntu

Make sure your package repository is up to date

```bash
sudo apt-get update
```

Subsequently install the ``build-essential`` package as well as the cross-compiler for the target you want to build.

```bash
sudo apt-get install build-essential gcc-arm-none-eabi gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

For AArch64 builds (`TARGET=raspi64`, `TARGET=pbpro`, `TARGET=virt`) one more setup step is required. The AArch64 architecture allows bi-endian operation, but the majority of the world is focusing on little-endian format as the native one. The cross-compiler provided by Ubuntu is not an exception and it defaults to little-endian systems. Furthermore, it lacks one big-endian relevant header which will need to be fixed now

```bash
sudo cp /usr/aarch64-linux-gnu/include/gnu/stubs-lp64.h /usr/aarch64-linux-gnu/include/gnu/stubs-lp64_be.h
```

One might wonder how it could work if a little-endian header is taken for the big-endian target. Well, in this case it is fully possible - this file is almost empty. At this point everything is configured properly and building of Emu68 can start. First, clone the repository

```bash
git clone https://github.com/michalsc/Emu68.git
```

After entering the ``Emu68`` directory created by git, one need to pull the submodules

```bash
git submodule update --init --recursive
```

Now, create build directory and install directory, enter the build directory

```bash
mkdir build install
cd build
```

Finally, configure the cmake project. For the restored 32-bit Raspberry Pi target:

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=../install -DTARGET=raspi \
         -DCMAKE_TOOLCHAIN_FILE=../toolchains/arm-elf.cmake
```

For a 64-bit Raspberry Pi PiStorm build:

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=../install -DTARGET=raspi64 -DVARIANT=pistorm \
				 -DCMAKE_TOOLCHAIN_FILE=../toolchains/aarch64-linux-gnu.cmake
```

Should you prefer a 64-bit Raspberry Pi build which works in bare metal but does not require PiStorm board, remove the ``-DVARIANT=pistorm`` from the second line. If you prefer to not use the toolchain file, you need to specify your preferred compiler by yourself, e.g.

```bash
CC=aarch64-linux-gnu-gcc CXX=aarch64-linux-gnu-g++ cmake .. -DCMAKE_INSTALL_PREFIX=../install \
         -DTARGET=raspi64 -DVARIANT=pistorm
```

During configuration process cmake will fetch the most recent RaspberryPi firmware by itself. After the configuration is completed, start building process and finally install the compiled files to previously created ``install`` directory

```bash
make && make install
```

Now, build process is completed. Copy the contents of the install directory onto FAT32 or FAT16 formatted SD card. Your Emu68 build is completed.

## ARM32 QEMU smoke test

The restored `TARGET=raspi` path includes a bounded QEMU smoke test for early ARM32 bring-up:

```bash
scripts/run-qemu-raspi32-smoke.sh
```

This helper injects a minimal device tree, boot shim, and valid one-block HUNK payload on top of the `raspi2b` machine so the bare-metal ARM32 image can execute translated m68k code under the JIT. The default payload runs `moveq #42,d0; rts`, and the harness checks that execution returns with `D0 = 0x0000002a`. Set `EMU68_QEMU_PAYLOAD=loop10` to run a small branch loop that returns with `D0 = 0x0000000a`. Use `EMU68_QEMU_TIMEOUT=<seconds>` to extend the observation window, for example:

```bash
EMU68_QEMU_TIMEOUT=12 scripts/run-qemu-raspi32-smoke.sh
```

Set `EMU68_QEMU_PAYLOAD=bsr4` to cover an inlined `bsr/rts` subroutine path that returns with `D0 = 0x00000004`, `EMU68_QEMU_PAYLOAD=stack42` to cover a translated stack push/pop round-trip that still returns `D0 = 0x0000002a`, `EMU68_QEMU_PAYLOAD=a0mem42` to cover an `A0` post-increment/pre-decrement memory round-trip against the framebuffer base while still returning `D0 = 0x0000002a`, `EMU68_QEMU_PAYLOAD=pea_a0` to cover `PEA (A0)` plus stack pop, which returns `D0 = 0x3c100000` and can also assert that `A7` returns to `0x3f7ff000`, `EMU68_QEMU_PAYLOAD=cmpiz42` to cover `cmpi.b #$2a, d0` and assert the final `SR = ..Z..` zero-flag state, `EMU68_QEMU_PAYLOAD=cmpibeq7` to cover `cmpi.b #$2a, d0; beq` and assert that the taken branch returns `D0 = 0x00000007`, `EMU68_QEMU_PAYLOAD=dbf3` to cover a three-iteration `dbra d1,<loop>` countdown that returns `D0 = 0x00000003` and leaves `D1 = 0x0000ffff`, `EMU68_QEMU_PAYLOAD=dbne9` to cover the complementary `dbne d1,<skip>` fallthrough path where the condition is already true, returning `D0 = 0x00000009` with `D1 = 0x00000002` unchanged, `EMU68_QEMU_PAYLOAD=dbt7` to cover the `dbt` no-op case, which ignores the displacement, falls through, and returns `D0 = 0x00000007` while preserving `D1 = 0x00000002`, `EMU68_QEMU_PAYLOAD=snememff` to cover `sne.b (a0)` storing `0xff` through the memory-destination `Scc` path and then reading it back, `EMU68_QEMU_PAYLOAD=stmemff` to cover the unconditional `st.b (a0)` fast path storing `0xff`, `EMU68_QEMU_PAYLOAD=sfmem00` to cover the unconditional `sf.b (a0)` fast path storing `0x00`, `EMU68_QEMU_PAYLOAD=sneregff` and `EMU68_QEMU_PAYLOAD=snereg00` to cover the conditional `sne d0` register path with both true and false outcomes, `EMU68_QEMU_PAYLOAD=stregff` and `EMU68_QEMU_PAYLOAD=sfreg00` to cover the unconditional `st d0` and `sf d0` register fast paths, `EMU68_QEMU_PAYLOAD=trapf_w8`, `EMU68_QEMU_PAYLOAD=trapf_l9`, and `EMU68_QEMU_PAYLOAD=trapf_n11` to cover the non-trapping `trapf` word, long, and no-operand forms, `EMU68_QEMU_PAYLOAD=movecvbr15` to cover the standalone ARM32 `movec a1,vbr` path and leave `VBR = 0x3f6fefb4`, `EMU68_QEMU_PAYLOAD=moveuspfb` to cover both privileged ARM32 `move a0,usp` and `move usp,a0`, returning `D0 = 0x3c100000` while keeping active `A7 = 0x3f7ff000`, `EMU68_QEMU_PAYLOAD=movesr2000` to cover privileged ARM32 `move sr,d0`, returning `D0 = 0x00002700` while leaving active `A7 = 0x3f7ff000`, `EMU68_QEMU_PAYLOAD=movetosr2004` to cover privileged ARM32 `move #$2004,sr`, returning `D0 = 0x00002004` and proving that `SEQ` sees the injected `Z` flag by leaving `D1 = 0x000002ff` with active `A7 = 0x3f7ff000`, `EMU68_QEMU_PAYLOAD=moveccr04` to cover ARM32 `move d0,ccr` plus `move ccr,d0`, returning `D0 = 0x00000004` and proving that `SEQ` sees the injected `Z` flag by leaving `D1 = 0x000002ff`, `EMU68_QEMU_PAYLOAD=nbcd96` to cover register-form ARM32 `nbcd d0`, returning `D0 = 0x00000096` with `D1 = 0x00000011`, `EMU68_QEMU_PAYLOAD=nbcd90h` to cover the ARM32 `nbcd d0` high-digit carry path on `0x10 -> 0x90`, returning `D0 = 0x00000090` with `D1 = 0x00000011`, `EMU68_QEMU_PAYLOAD=nbcd0z` to cover the zero-result `nbcd d0` path that preserves an incoming `Z` flag, returning `D0 = 0x00000000` with `D1 = 0x000000ff` and `D2 = 0x00000004`, `EMU68_QEMU_PAYLOAD=nbcd99x` to cover the ARM32 `X`-input `nbcd d0` path after `move d1,ccr`, returning `D0 = 0x00000099` with `D1 = 0x00000011`, `EMU68_QEMU_PAYLOAD=nbcdpre96` to cover the `-(a0)` memory `NBCD` path and restore `A0 = 0x3c100000` after reading back `0x96`, `EMU68_QEMU_PAYLOAD=abcd34` to cover register-form ARM32 `abcd d0,d1`, returning `D1 = 0x00000034` with `D2 = 0x00000000`, `EMU68_QEMU_PAYLOAD=abcd00x` to cover the ARM32 `X`-input register path on `0x00 + 0x99 + X`, returning `D1 = 0x00000000` with `D2 = 0x00000015`, `EMU68_QEMU_PAYLOAD=abcdpre1` to cover the `-(a0),-(a1)` memory `ABCD` path seeded with two zero bytes plus an incoming `X`, returning `D0 = 0x000000ff` while leaving `A1 = 0x3c100000`, `EMU68_QEMU_PAYLOAD=packd1234`, `EMU68_QEMU_PAYLOAD=packd1239`, and `EMU68_QEMU_PAYLOAD=packd1231` to cover ARM32 `pack d0,d1,#<adjustment>` with zero, positive, and negative adjustments, leaving `D1 = 0x00000234`, `0x00000239`, and `0x00000231`, `EMU68_QEMU_PAYLOAD=unpkd1304`, `EMU68_QEMU_PAYLOAD=unpkd1309`, and `EMU68_QEMU_PAYLOAD=unpkd1301` to cover the matching `unpk d0,d1,#<adjustment>` forms and leave `D1 = 0x00000304`, `0x00000309`, and `0x00000301`, `EMU68_QEMU_PAYLOAD=trapt13` to cover a taken `trapt` path that programs `VBR` with `movec`, builds an in-HUNK vector entry, returns through a real ARM32 `rte`, and then exits with `D0 = 0x0000000d` while restoring `A7 = 0x3f7ff000`, `EMU68_QEMU_PAYLOAD=trapne14` to cover the conditional taken `trapne` path through the same vector machinery and return `D0 = 0x0000000e` with the same final `A7` restoration, `EMU68_QEMU_PAYLOAD=casb_ok21` and `EMU68_QEMU_PAYLOAD=casb_fail13` to cover ARM32 `cas.b d0,d1,(a0)` on both the success and compare-register writeback paths, `EMU68_QEMU_PAYLOAD=casw_ok5678` and `EMU68_QEMU_PAYLOAD=casl_ok89abcdef` to cover the aligned ARM32 `cas.w` and `cas.l` store paths, and `EMU68_QEMU_PAYLOAD=cas2w_ok2244` plus `EMU68_QEMU_PAYLOAD=cas2l_fail3333` to cover the paired ARM32 `cas2.w` success case and the `cas2.l` second-operand mismatch/writeback path that leaves both `D0` and memory at `0x33333333`, `EMU68_QEMU_PAYLOAD=addqb79`, `EMU68_QEMU_PAYLOAD=subqb77`, `EMU68_QEMU_PAYLOAD=addqw0000`, `EMU68_QEMU_PAYLOAD=subqwffff`, `EMU68_QEMU_PAYLOAD=addqa8`, and `EMU68_QEMU_PAYLOAD=subqa4` to cover `ADDQ`/`SUBQ` byte, word, and address-register forms in the ARM32 line-5 translator, `EMU68_QEMU_PAYLOAD=addqb_mem1` and `EMU68_QEMU_PAYLOAD=subqb_mem1` to cover the direct byte-memory `(A0)` quick-arithmetic path, `EMU68_QEMU_PAYLOAD=addqb_postinc`, `EMU68_QEMU_PAYLOAD=subqb_postinc`, `EMU68_QEMU_PAYLOAD=addqb_predec`, and `EMU68_QEMU_PAYLOAD=subqb_predec` to cover the byte-sized memory `(A0)+` and `-(A0)` quick-arithmetic paths, `EMU68_QEMU_PAYLOAD=addqb_a7_postinc`, `EMU68_QEMU_PAYLOAD=subqb_a7_postinc`, `EMU68_QEMU_PAYLOAD=addqb_a7_predec`, and `EMU68_QEMU_PAYLOAD=subqb_a7_predec` to cover the byte-sized `(A7)+` and `-(A7)` stack-pointer special case, which restores `A7` to `0x3f7ff000` after the translated round-trip, `EMU68_QEMU_PAYLOAD=addql_mem1`, `EMU68_QEMU_PAYLOAD=subql_mem1`, `EMU68_QEMU_PAYLOAD=addql_postinc`, `EMU68_QEMU_PAYLOAD=subql_postinc`, `EMU68_QEMU_PAYLOAD=addql_predec`, and `EMU68_QEMU_PAYLOAD=subql_predec` to cover the 32-bit memory `ADDQ`/`SUBQ` paths for direct `(A0)`, `(A0)+`, and `-(A0)` operands, and `EMU68_QEMU_PAYLOAD=addqw_mem1`, `EMU68_QEMU_PAYLOAD=subqw_mem1`, `EMU68_QEMU_PAYLOAD=addqw_postinc`, `EMU68_QEMU_PAYLOAD=subqw_postinc`, `EMU68_QEMU_PAYLOAD=addqw_predec`, and `EMU68_QEMU_PAYLOAD=subqw_predec` to cover the corresponding 16-bit memory paths. The register `Scc` tests preserve the upper 24 bits and return `D0 = 0x123456ff` or `D0 = 0x12345600`, the `trapf` tests prove PC advancement by returning `D0 = 0x00000008`, `0x00000009`, and `0x0000000b`, the `movec`, `move usp`, `move sr`, `move to sr`, `move ccr`, `nbcd`, `abcd`, `pack`, `unpk`, taken `TRAPcc`, `CAS`, and `CAS2` tests prove that ARM32 now writes `VBR` correctly, routes through a `movec`-programmed in-HUNK vector handler with a real `rte` return, transfers `USP` without disturbing the active supervisor stack, exposes the supervisor SR word through the translated `MOVE from SR` path, updates the live supervisor SR through the translated `MOVE to SR` path, updates live condition evaluation through the translated CCR paths, preserves `Z` across the zero-result `NBCD` and carry-out `ABCD` cases, carries `X` into both decimal negation and decimal addition paths, handles the predecrement memory `NBCD` and `ABCD` forms, covers the low-nibble-zero high-digit `NBCD` correction path, performs register-form BCD pack/unpack across zero, positive, and negative adjustment words without falling back to the old undefined-instruction stub, atomically swaps byte, word, and long values through translated `CAS`, and preserves compare-register writeback and paired-memory semantics through translated `CAS2`, the quick byte tests preserve upper bits while returning `D0 = 0x12345679` and `0x12345677`, the word tests assert `SR = X.Z.C` and `SR = XN..C` on `D0 = 0x12340000` and `0x1234ffff`, the address-register tests prove `A0` updates alongside `D0 = 0x3c100008` and `0x3c0ffffc`, and the memory quick-arithmetic tests round-trip through translated loads and stores to return `D0 = 0x000000ff` or `0x00000001` while restoring `A0 = 0x3c100000`, including the direct byte-memory case, the byte-sized `A7` update-by-2 special case, and the separate 16-bit and 32-bit memory paths. By default the smoke test treats `[JIT] Back from translated code` and the payload-specific result marker as the success conditions, then exits cleanly on the bounded timeout. Override them with `EMU68_QEMU_EXPECT=<text>` and `EMU68_QEMU_EXPECT_RESULT=<text>` if you want to probe a different boot stage or payload result. Use `EMU68_QEMU_BOOTARGS="<args>"` to exercise specific ARM32 boot-time paths such as `enable_cache`, `debug`, or `disassemble` without editing the checked-in DTS template, `EMU68_QEMU_EXPECT_EXTRA=<text>` to require one additional log marker anywhere in the run, and `EMU68_QEMU_EXPECT_POST=<text>` to require a marker after the main success marker, which is useful for final-state checks like restored `A0` or `A7`.

The `MOVES` coverage also includes `EMU68_QEMU_PAYLOAD=movesb_store33`, `EMU68_QEMU_PAYLOAD=movesb_load7f`, `EMU68_QEMU_PAYLOAD=movesb_loada80`, `EMU68_QEMU_PAYLOAD=movesb_posta0`, `EMU68_QEMU_PAYLOAD=movesb_prea0`, `EMU68_QEMU_PAYLOAD=movesl_storeef`, `EMU68_QEMU_PAYLOAD=movesl_load78`, and `EMU68_QEMU_PAYLOAD=movesw_loada34` to exercise privileged byte, word, and long transfers, address-register sign extension, and the same-register `(A0)+` and `-(A0)` auto-update cases.

The line-4 control-flow coverage also includes `EMU68_QEMU_PAYLOAD=bkptret11` and `EMU68_QEMU_PAYLOAD=illegalret12` to cover `BKPT #0` and `ILLEGAL` vectoring through an in-HUNK illegal-instruction handler and back to the following instruction, `EMU68_QEMU_PAYLOAD=rtdspfb` to cover `RTD #4` cleaning a stacked longword argument so a framebuffer marker word is exposed at the top of stack, returning `D0 = 0x3c1000ff` and leaving `A7 = 0x3f7ff004` after the marker pop, `EMU68_QEMU_PAYLOAD=rtrret05` to cover an `RTR` control transfer that restores CCR and PC from a manually built stack frame before returning through the original `BSR` call chain, `EMU68_QEMU_PAYLOAD=nopret06` to cover the translated `NOP` fast path by returning `D0 = 0x00000006` after a following `ADDQ`, `EMU68_QEMU_PAYLOAD=trapv15` to cover a taken `TRAPV` path that sets overflow in translated code, vectors through an in-HUNK handler, and returns with `A7` restored to the supervisor stack top, and `EMU68_QEMU_PAYLOAD=trap3ret07` to cover plain `TRAP #3` vectoring through a `movec`-programmed in-HUNK handler before returning with `D0 = 0x00000007`, `D1 = 0x00000017`, and restored `A7 = 0x3f7ff000`.

The line-4 data-path coverage also includes `EMU68_QEMU_PAYLOAD=swap5678`, `EMU68_QEMU_PAYLOAD=lea8a0`, `EMU68_QEMU_PAYLOAD=movemld12`, `EMU68_QEMU_PAYLOAD=movemlda0`, `EMU68_QEMU_PAYLOAD=movemst12`, `EMU68_QEMU_PAYLOAD=movempost12`, `EMU68_QEMU_PAYLOAD=movempre12`, `EMU68_QEMU_PAYLOAD=movemprea0`, `EMU68_QEMU_PAYLOAD=movemposta0`, `EMU68_QEMU_PAYLOAD=movemwd80`, `EMU68_QEMU_PAYLOAD=movemwda0`, `EMU68_QEMU_PAYLOAD=movemwpost23`, `EMU68_QEMU_PAYLOAD=movemwposta0`, `EMU68_QEMU_PAYLOAD=movemwpre23`, `EMU68_QEMU_PAYLOAD=movemwprea0`, `EMU68_QEMU_PAYLOAD=extwff80`, `EMU68_QEMU_PAYLOAD=extl8034`, `EMU68_QEMU_PAYLOAD=extbf80`, `EMU68_QEMU_PAYLOAD=extf80`, `EMU68_QEMU_PAYLOAD=linkwsp`, `EMU68_QEMU_PAYLOAD=linklsp`, and `EMU68_QEMU_PAYLOAD=unlka44` to exercise `SWAP`, translated `LEA` with both displacement and register-indirect address calculation, direct `MOVEM.L` register-list loads from PC-relative data, direct `MOVEM.L (A0),A0-A1` base-register loads, direct `MOVEM.L` register-list stores and reloads through `(A0)`, postincrement `MOVEM.L (A0)+,D1-D2`, predecrement `MOVEM.L D0-D1,-(A0)`, predecrement `MOVEM.L A0-A1,-(A0)` with the base register in the source list and the decremented base value written to memory, postincrement `MOVEM.L (A0)+,A0-A1` with the base register in the load list, direct `MOVEM.W` store/load sign-extension through `(A0)`, direct `MOVEM.W (A0),A0-A1` base-register sign-extending loads, postincrement `MOVEM.W (A0)+,D2-D3`, postincrement `MOVEM.W (A0)+,A0-A1` with the base register in the load list, predecrement `MOVEM.W D0-D1,-(A0)`, predecrement `MOVEM.W A0-A1,-(A0)` with the base register in the source list and the decremented base value written to memory, all three `EXT` forms, the `EXT.W`+`EXT.L` fusion path, both `LINK.W` and `LINK.L` paired with `UNLK`, and a standalone `UNLK A0` frame-pop path that restores `A7 = 0x3f7ff000` while returning `D0 = 0x11223344`.

The line-8/9/C arithmetic coverage also includes `EMU68_QEMU_PAYLOAD=exgd01`, `EMU68_QEMU_PAYLOAD=cmpmff`, `EMU68_QEMU_PAYLOAD=sbcd99x`, `EMU68_QEMU_PAYLOAD=sbcdpre99x`, `EMU68_QEMU_PAYLOAD=addx01`, `EMU68_QEMU_PAYLOAD=addxpre01`, `EMU68_QEMU_PAYLOAD=subxff`, and `EMU68_QEMU_PAYLOAD=subxpreff` to exercise `EXG`, `CMPM`, `SBCD`, `ADDX`, and `SUBX` on the ARM32 translator, including the predecrement memory forms that consume an incoming `X` flag and drive the paired `A0`/`A1` update paths.

That same block now includes `EMU68_QEMU_PAYLOAD=exgaa01` and `EMU68_QEMU_PAYLOAD=exgdafb` to cover the remaining `EXG` address/address and data/address forms.

The `Scc` matrix also includes `EMU68_QEMU_PAYLOAD=snemem00`, `EMU68_QEMU_PAYLOAD=snepostff`, `EMU68_QEMU_PAYLOAD=snepost00`, `EMU68_QEMU_PAYLOAD=snepreff`, `EMU68_QEMU_PAYLOAD=snepre00`, `EMU68_QEMU_PAYLOAD=snea7postff`, `EMU68_QEMU_PAYLOAD=snea7post00`, `EMU68_QEMU_PAYLOAD=snea7preff`, and `EMU68_QEMU_PAYLOAD=snea7pre00` to cover direct false stores plus `(A0)+`, `-(A0)`, `(A7)+`, and `-(A7)` memory-destination `sne.b` paths, including the byte-sized stack-pointer update-by-2 special case.

For example, this verifies that the ARM32 cache-enable bootarg reaches JIT execution and leaves `CACR=0x80008000` visible in the context dump:

```bash
EMU68_QEMU_BOOTARGS="console=ttyAMA0 skip_reloc enable_cache" \
EMU68_QEMU_EXPECT_EXTRA="CACR=0x80008000" \
scripts/run-qemu-raspi32-smoke.sh
```

You can also probe the ARM32 disassembly path without editing the DTS template:

```bash
EMU68_QEMU_BOOTARGS="console=ttyAMA0 skip_reloc disassemble" \
EMU68_QEMU_EXPECT_EXTRA='moveq   #$2a, d0' \
scripts/run-qemu-raspi32-smoke.sh
```

For `TARGET=raspi` builds configured on a host with `qemu-system-arm`, `dtc`, and the ARM bare-metal toolchain installed, the same smoke coverage is also registered as `ctest` tests `raspi-arm32-qemu-smoke`, `raspi-arm32-qemu-loop-smoke`, `raspi-arm32-qemu-bsr-smoke`, `raspi-arm32-qemu-cache-smoke`, `raspi-arm32-qemu-disasm-smoke`, `raspi-arm32-qemu-stack-smoke`, `raspi-arm32-qemu-a0mem-smoke`, `raspi-arm32-qemu-pea-smoke`, `raspi-arm32-qemu-cmpi-smoke`, `raspi-arm32-qemu-cmpi-branch-smoke`, `raspi-arm32-qemu-dbf-smoke`, `raspi-arm32-qemu-dbne-smoke`, `raspi-arm32-qemu-dbt-smoke`, `raspi-arm32-qemu-scc-mem-smoke`, `raspi-arm32-qemu-scc-true-smoke`, `raspi-arm32-qemu-scc-false-smoke`, `raspi-arm32-qemu-scc-reg-sne-smoke`, `raspi-arm32-qemu-scc-reg-sne-false-smoke`, `raspi-arm32-qemu-scc-reg-st-smoke`, `raspi-arm32-qemu-scc-reg-sf-smoke`, `raspi-arm32-qemu-trapf-word-smoke`, `raspi-arm32-qemu-trapf-long-smoke`, `raspi-arm32-qemu-trapf-none-smoke`, `raspi-arm32-qemu-movec-vbr-smoke`, `raspi-arm32-qemu-moveusp-smoke`, `raspi-arm32-qemu-movesr-smoke`, `raspi-arm32-qemu-movetosr-smoke`, `raspi-arm32-qemu-moveccr-smoke`, `raspi-arm32-qemu-nbcd-smoke`, `raspi-arm32-qemu-nbcd-high-smoke`, `raspi-arm32-qemu-nbcd-zero-smoke`, `raspi-arm32-qemu-nbcd-x-smoke`, `raspi-arm32-qemu-nbcd-predec-smoke`, `raspi-arm32-qemu-abcd-smoke`, `raspi-arm32-qemu-abcd-x-smoke`, `raspi-arm32-qemu-abcd-predec-smoke`, `raspi-arm32-qemu-pack-smoke`, `raspi-arm32-qemu-pack-add-smoke`, `raspi-arm32-qemu-pack-sub-smoke`, `raspi-arm32-qemu-unpk-smoke`, `raspi-arm32-qemu-unpk-add-smoke`, `raspi-arm32-qemu-unpk-sub-smoke`, `raspi-arm32-qemu-trapt-smoke`, `raspi-arm32-qemu-trapne-smoke`, `raspi-arm32-qemu-cas-byte-smoke`, `raspi-arm32-qemu-cas-byte-fail-smoke`, `raspi-arm32-qemu-cas-word-smoke`, `raspi-arm32-qemu-cas-long-smoke`, `raspi-arm32-qemu-cas2-word-smoke`, `raspi-arm32-qemu-cas2-long-fail-smoke`, `raspi-arm32-qemu-addq-byte-smoke`, `raspi-arm32-qemu-subq-byte-smoke`, `raspi-arm32-qemu-addq-word-smoke`, `raspi-arm32-qemu-subq-word-smoke`, `raspi-arm32-qemu-addq-addr-smoke`, `raspi-arm32-qemu-subq-addr-smoke`, `raspi-arm32-qemu-addq-postinc-mem-smoke`, `raspi-arm32-qemu-subq-postinc-mem-smoke`, `raspi-arm32-qemu-addq-predec-mem-smoke`, `raspi-arm32-qemu-subq-predec-mem-smoke`, `raspi-arm32-qemu-addq-a7-postinc-mem-smoke`, `raspi-arm32-qemu-subq-a7-postinc-mem-smoke`, `raspi-arm32-qemu-addq-a7-predec-mem-smoke`, `raspi-arm32-qemu-subq-a7-predec-mem-smoke`, `raspi-arm32-qemu-addq-long-mem-smoke`, `raspi-arm32-qemu-subq-long-mem-smoke`, `raspi-arm32-qemu-addq-long-postinc-mem-smoke`, `raspi-arm32-qemu-subq-long-postinc-mem-smoke`, `raspi-arm32-qemu-addq-long-predec-mem-smoke`, `raspi-arm32-qemu-subq-long-predec-mem-smoke`, `raspi-arm32-qemu-addq-word-mem-smoke`, `raspi-arm32-qemu-subq-word-mem-smoke`, `raspi-arm32-qemu-addq-word-postinc-mem-smoke`, `raspi-arm32-qemu-subq-word-postinc-mem-smoke`, `raspi-arm32-qemu-addq-word-predec-mem-smoke`, `raspi-arm32-qemu-subq-word-predec-mem-smoke`, `raspi-arm32-qemu-addq-byte-mem-smoke`, and `raspi-arm32-qemu-subq-byte-mem-smoke`.

That matrix also registers `raspi-arm32-qemu-moves-store-smoke`, `raspi-arm32-qemu-moves-load-smoke`, `raspi-arm32-qemu-moves-load-addr-smoke`, `raspi-arm32-qemu-moves-postinc-a0-smoke`, `raspi-arm32-qemu-moves-predec-a0-smoke`, `raspi-arm32-qemu-moves-long-store-smoke`, `raspi-arm32-qemu-moves-long-load-smoke`, and `raspi-arm32-qemu-moves-word-load-addr-smoke`.

That matrix also registers `raspi-arm32-qemu-exg-smoke`, `raspi-arm32-qemu-cmpm-smoke`, `raspi-arm32-qemu-sbcd-smoke`, `raspi-arm32-qemu-sbcd-predec-smoke`, `raspi-arm32-qemu-addx-smoke`, `raspi-arm32-qemu-addx-predec-smoke`, `raspi-arm32-qemu-subx-smoke`, and `raspi-arm32-qemu-subx-predec-smoke`.

It also registers `raspi-arm32-qemu-exg-aa-smoke` and `raspi-arm32-qemu-exg-da-smoke`.

The extended `Scc` coverage is registered as `raspi-arm32-qemu-scc-mem-false-smoke`, `raspi-arm32-qemu-scc-postinc-smoke`, `raspi-arm32-qemu-scc-postinc-false-smoke`, `raspi-arm32-qemu-scc-predec-smoke`, `raspi-arm32-qemu-scc-predec-false-smoke`, `raspi-arm32-qemu-scc-a7-postinc-smoke`, `raspi-arm32-qemu-scc-a7-postinc-false-smoke`, `raspi-arm32-qemu-scc-a7-predec-smoke`, and `raspi-arm32-qemu-scc-a7-predec-false-smoke`.
