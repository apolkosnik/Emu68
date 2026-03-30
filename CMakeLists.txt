cmake_minimum_required(VERSION 3.14.0)
project(Emu68 VERSION 1.0.7)
set(PROJECT_VERSION 1.0.7)
include(CTest)

include(cmake/verstring.cmake)
include(cmake/firmware_download.cmake)
get_verstring(VERSTRING)
get_git_sha(GIT_SHA)

set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

set(SUPPORTED_TARGETS "raspi" "raspi64" "pbpro" "rockpro64" "virt")
set(TARGET "raspi64" CACHE STRING "One of target machines: ${SUPPORTED_TARGETS}")
set_property(CACHE TARGET PROPERTY STRINGS ${SUPPORTED_TARGETS})

set(SUPPORTED_VARIANTS "none" "pistorm" "pistorm32lite")
set(VARIANT "none" CACHE STRING "Compilation variant: ${SUPPORTED_VARIANTS}")
set_property(CACHE VARIANT PROPERTY STRINGS ${SUPPORTED_VARIANTS})

set(ARCH_FILES "")
set(TARGET_FILES "")
set(BASE_FILES
    src/support.c
    src/tlsf.c
    src/devicetree.c
    src/cpp_support.cpp
    src/EmuLogo.c
    src/HunkLoader.c
    src/ElfLoader.c
    src/md5.c
    src/disasm.c
    src/findtoken.c
    src/cache.c
)

set(EMU68_FILES
    src/M68k_Translator.c
    src/M68k_SR.c
    src/M68k_MULDIV.c
    src/M68k_MOVE.c
    src/M68k_EA.c
    src/M68k_LINE0.c
    src/M68k_LINE4.c
    src/M68k_LINE5.c
    src/M68k_LINE6.c
    src/M68k_LINE8.c
    src/M68k_LINE9.c
    src/M68k_LINEB.c
    src/M68k_LINEC.c
    src/M68k_LINED.c
    src/M68k_LINEE.c
    src/M68k_LINEF.c
    src/M68k_Exception.c
    src/M68k_CC.c
    
    src/math/__rem_pio2.c
    src/math/__rem_pio2_large.c
    src/math/__tan.c
    src/math/__sin.c
    src/math/__cos.c
    src/math/__expo2.c
    src/math/atan.c
    src/math/atanh.c
    src/math/acos.c
    src/math/asin.c
    src/math/tan.c
    src/math/tanh.c
    src/math/scalbn.c
    src/math/floor.c
    src/math/cosh.c
    src/math/exp.c
    src/math/exp10.c
    src/math/exp2.c
    src/math/expm1.c
    src/math/log.c
    src/math/log1p.c
    src/math/log10.c
    src/math/log2.c
    src/math/sinh.c
    src/math/pow.c
    src/math/modf.c
    src/math/sincos.c
    src/math/sin.c
    src/math/cos.c
    src/math/remquo.c
    src/math/96bit.c
)

if (${TARGET} IN_LIST SUPPORTED_TARGETS)
    message("-- Selected target machine: ${TARGET}")
    if(${TARGET} STREQUAL "raspi")
        set(TARGET_ARCH "armhf")
        set(EMU68_ARM_64BIT 0)
        list(APPEND TARGET_FILES src/raspi/start_rpi.c)
        list(APPEND TARGET_FILES src/raspi/support_rpi.c)
        download_raspi_firmware()
        add_compile_definitions(RASPI)
    elseif(${TARGET} STREQUAL "raspi64")
        set(TARGET_ARCH "aarch64")
        set(EMU68_ARM_64BIT 1)
        list(APPEND TARGET_FILES src/raspi/start_rpi64.c)
        list(APPEND TARGET_FILES src/raspi/support_rpi.c)
        list(APPEND TARGET_FILES src/raspi/topaz.c)
        download_raspi_firmware()
        add_compile_definitions(RASPI)
    elseif(${TARGET} STREQUAL "pbpro")
        set(TARGET_ARCH "aarch64")
        set(EMU68_ARM_64BIT 1)
        list(APPEND TARGET_FILES
            src/pine64/start_pbpro.c
            src/pine64/support_pbpro.c
        )
        install(FILES ${CMAKE_SOURCE_DIR}/blobs/rk3399-pinebookpro.dtb DESTINATION .)
        install(FILES ${CMAKE_BINARY_DIR}/Emu68.img DESTINATION .)
        install(FILES ${CMAKE_SOURCE_DIR}/scripts/extlinux.conf DESTINATION extlinux/)
    elseif(${TARGET} STREQUAL "virt")
        set(TARGET_ARCH "aarch64")
        set(EMU68_ARM_64BIT 1)
        list(APPEND TARGET_FILES
            src/virt/start_virt.c
            src/virt/support_virt.c
        )
        install(FILES ${CMAKE_BINARY_DIR}/Emu68.img DESTINATION .)
    endif()
else()
    message(FATAL_ERROR "Wrong target machine specified: ${TARGET}")
endif()

if (${VARIANT} IN_LIST SUPPORTED_VARIANTS)
    message("-- Selected variant: ${VARIANT}")
    if(${VARIANT} STREQUAL "pistorm" OR ${VARIANT} STREQUAL "pistorm32lite")
        if(NOT ${TARGET} STREQUAL "raspi64")
            message(FATAL_ERROR "PiStorm variants are supported on AArch64 Raspberry Pi targets only.")
        endif()
        add_compile_definitions(PISTORM)
        if(${VARIANT} STREQUAL "pistorm32lite")
            add_compile_definitions(PISTORM32LITE)
            add_compile_definitions(PISTORM32)
            list(APPEND BASE_FILES
                src/pistorm/ps32_protocol.c
            )
        else()
            list(APPEND BASE_FILES
                src/pistorm/ps_protocol.c
            )
        endif()
        include_directories(src/pistorm)
        list(APPEND BASE_FILES
            src/boards/devicetree.c
            src/boards/z2ram.c                
            src/boards/sdcard.c
            src/boards/68040.c
            src/boards/emmc.c
            src/boards/unicam.c
        )
    endif()
    
else()
    message(FATAL_ERROR "Wrong variant selected: ${VARIANT}")
endif()

if(${TARGET} STREQUAL "raspi" OR ${TARGET} STREQUAL "raspi64")
    if(${VARIANT} STREQUAL "pistorm" OR ${VARIANT} STREQUAL "pistorm32lite")
        configure_file(scripts/config_pistorm.txt scripts/config.txt @ONLY)
        install(FILES ${CMAKE_BINARY_DIR}/Emu68.img DESTINATION . RENAME Emu68-${VARIANT})
    else()
        configure_file(scripts/config.txt scripts/config.txt @ONLY)
        install(FILES ${CMAKE_BINARY_DIR}/Emu68.img DESTINATION .)
    endif()
    install(FILES ${CMAKE_BINARY_DIR}/scripts/config.txt DESTINATION .)
endif()

add_link_options(-Wl,--build-id -nostdlib -nostartfiles -static)
add_compile_options(-mbig-endian -fno-exceptions -ffreestanding -Wall -Wextra -Werror)

if(${TARGET_ARCH} STREQUAL "armhf")
    set(ARMHF_LEGACY_FILES
        src/armhf_legacy/M68k_Translator.c
        src/armhf_legacy/M68k_SR.c
        src/armhf_legacy/M68k_MULDIV.c
        src/armhf_legacy/M68k_MOVE.c
        src/armhf_legacy/M68k_EA.c
        src/armhf_legacy/M68k_LINE0.c
        src/armhf_legacy/M68k_LINE4.c
        src/armhf_legacy/M68k_LINE5.c
        src/armhf_legacy/M68k_LINE6.c
        src/armhf_legacy/M68k_LINE8.c
        src/armhf_legacy/M68k_LINE9.c
        src/armhf_legacy/M68k_LINEB.c
        src/armhf_legacy/M68k_LINEC.c
        src/armhf_legacy/M68k_LINED.c
        src/armhf_legacy/M68k_LINEE.c
        src/armhf_legacy/M68k_LINEF.c
        src/armhf_legacy/M68k_Exception.c
        src/armhf_legacy/M68k_CC.c
    )
    list(FILTER EMU68_FILES EXCLUDE REGEX "^src/M68k_")
    list(APPEND EMU68_FILES ${ARMHF_LEGACY_FILES})
    set_source_files_properties(${ARMHF_LEGACY_FILES} PROPERTIES COMPILE_FLAGS
        "-Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-function -Wno-overflow -Wno-shift-count-overflow")
    list(APPEND ARCH_FILES
        src/armhf/start.c
        src/armhf/RegisterAllocator.c
    )
    set(CAPSTONE_ARM_SUPPORT ON CACHE BOOL "CAPSTONE_ARM_SUPPORT")
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/scripts/ldscript-be.lds)
    add_link_options(-Wl,--be8 -Wl,-Map -Wl,${CMAKE_BINARY_DIR}/Emu68.map -Wl,--format=elf32-bigarm -T ${LINKER_SCRIPT})
    add_compile_options(-mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4 -O3 -ffixed-r11 -fomit-frame-pointer)
else()
    set_source_files_properties(src/math/96bit.c PROPERTIES COMPILE_FLAGS 
        "-fcall-saved-x4 -fcall-saved-x5 -fcall-saved-x6 -fcall-saved-x7 -fcall-saved-x8 \
        -fcall-saved-x9 -fcall-saved-x10 -fcall-saved-x11 -fcall-saved-x12 -fcall-saved-x13 \
        -fcall-saved-x14 -fcall-saved-x15 -fcall-saved-x16 -fcall-saved-x17 -fcall-saved-x18")

    # ExecutionLoop breaks the C ABI for aarch64. All m68k registers are fixed (x13-x29)
    # The temporary arm code and PC are fixed too (x12, x18)
    set_source_files_properties(src/ExecutionLoop.c PROPERTIES COMPILE_FLAGS 
        "-fcall-used-x4 -fcall-used-x5 -fcall-used-x6 -fcall-used-x7 -fcall-used-x8 \
         -fcall-used-x9 -fcall-used-x10 -fcall-used-x11 -ffixed-x12 -ffixed-x18  \
         -ffixed-x19 -ffixed-x20 -ffixed-x21 -ffixed-x22 -ffixed-x23 -ffixed-x24 -ffixed-x25 -ffixed-x26  \
         -ffixed-x13 -ffixed-x14 -ffixed-x15 -ffixed-x16 -ffixed-x17 -ffixed-x27 -ffixed-x28 -ffixed-x29")

    list(APPEND ARCH_FILES
        src/aarch64/start.c
        src/aarch64/mmu.c
        src/aarch64/RegisterAllocator64.c
        src/aarch64/vectors.c
    )
    list(APPEND EMU68_FILES src/ExecutionLoop.c)
    set(CAPSTONE_ARM64_SUPPORT ON CACHE BOOL "CAPSTONE_ARM64_SUPPORT")
    set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/scripts/ldscript-be64.lds)
    add_link_options(-Wl,-EB -Wl,-Map -Wl,${CMAKE_BINARY_DIR}/Emu68.map -Wl,--format=elf64-bigaarch64 -T ${LINKER_SCRIPT})
    add_compile_options(-march=armv8-a+crc -mtune=cortex-a72 -fomit-frame-pointer -O3 -ffixed-x12 -ffixed-q31 -ffixed-q30 -ffixed-q29 -ffixed-q28)

    if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0)
        add_compile_options(-mno-outline-atomics)
    endif()
endif()

add_executable(Emu68.elf
    ${ARCH_FILES}
    ${TARGET_FILES}
    ${BASE_FILES}
    ${EMU68_FILES}
)

add_custom_command(
    TARGET Emu68.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary "${CMAKE_BINARY_DIR}/Emu68.elf" "${CMAKE_BINARY_DIR}/Emu68.img"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(external)

target_link_libraries(Emu68.elf capstone-static libdeflate_static)
target_include_directories(Emu68.elf PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/external/capstone/include)
target_include_directories(Emu68.elf PRIVATE ${CMAKE_BINARY_DIR}/include)
target_compile_definitions(Emu68.elf PRIVATE)

if(BUILD_TESTING AND "${TARGET}" STREQUAL "raspi")
    find_program(EMU68_QEMU_SYSTEM_ARM qemu-system-arm)
    find_program(EMU68_DTC dtc)
    find_program(EMU68_TIMEOUT timeout)
    find_program(EMU68_MKTEMP mktemp)
    find_program(EMU68_ARM_NONE_EABI_GCC arm-none-eabi-gcc)
    find_program(EMU68_ARM_NONE_EABI_OBJCOPY arm-none-eabi-objcopy)

    set(EMU68_SMOKE_MISSING_TOOLS "")
    if(NOT EMU68_QEMU_SYSTEM_ARM)
        list(APPEND EMU68_SMOKE_MISSING_TOOLS qemu-system-arm)
    endif()
    if(NOT EMU68_DTC)
        list(APPEND EMU68_SMOKE_MISSING_TOOLS dtc)
    endif()
    if(NOT EMU68_TIMEOUT)
        list(APPEND EMU68_SMOKE_MISSING_TOOLS timeout)
    endif()
    if(NOT EMU68_MKTEMP)
        list(APPEND EMU68_SMOKE_MISSING_TOOLS mktemp)
    endif()
    if(NOT EMU68_ARM_NONE_EABI_GCC)
        list(APPEND EMU68_SMOKE_MISSING_TOOLS arm-none-eabi-gcc)
    endif()
    if(NOT EMU68_ARM_NONE_EABI_OBJCOPY)
        list(APPEND EMU68_SMOKE_MISSING_TOOLS arm-none-eabi-objcopy)
    endif()

    if(EMU68_SMOKE_MISSING_TOOLS)
        list(JOIN EMU68_SMOKE_MISSING_TOOLS ", " EMU68_SMOKE_MISSING_TOOLS_TEXT)
        message(STATUS "Skipping ARM32 QEMU smoke test registration: missing host tools: ${EMU68_SMOKE_MISSING_TOOLS_TEXT}")
    else()
        add_test(
            NAME raspi-arm32-qemu-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-loop-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bsr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cache-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-disasm-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-stack-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-a0mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-pea-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-branch-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-dbf-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-dbne-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-dbt-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-true-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-false-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-reg-sne-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-reg-sne-false-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-reg-st-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-reg-sf-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-trapf-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-trapf-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-trapf-none-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movec-vbr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-moveusp-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movesr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movetosr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-stop-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bkpt-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-illegal-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-rtd-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-rtr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-trap3-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-nop-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-swap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-lea-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-load-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-base-load-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-store-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-postinc-load-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-predec-store-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-predec-base-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-postinc-base-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-word-base-load-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-word-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-word-postinc-base-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-word-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movem-word-predec-base-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-ext-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-ext-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-extb-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-ext-fused-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-link-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-link-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-clr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-not-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-neg-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-tst-zero-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-tst-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-tas-reg-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-tas-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-clr-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-not-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-neg-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-tst-mem-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-unlk-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-reset-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-moveccr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-moves-store-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-moves-load-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-moves-load-addr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-moves-postinc-a0-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-moves-predec-a0-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-moves-long-store-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-moves-long-load-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-moves-word-load-addr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-nbcd-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-nbcd-high-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-nbcd-zero-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-nbcd-x-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-nbcd-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-abcd-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-abcd-x-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-abcd-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-exg-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-exg-aa-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-exg-da-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpm-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sbcd-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sbcd-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addx-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addx-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subx-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subx-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-pack-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-pack-add-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-pack-sub-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-unpk-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-unpk-add-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-unpk-sub-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-trapt-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-trapne-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-trapv-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-chk-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-chk-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movep-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movep-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-chk2-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-byte-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-byte-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas2-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas2-long-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-byte-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-byte-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-addr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-addr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-a7-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-a7-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-a7-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-a7-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-long-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-long-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-long-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-long-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-long-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-long-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-word-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-word-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-word-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-word-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-word-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-word-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addq-byte-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subq-byte-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-mem-false-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-postinc-false-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-predec-false-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-a7-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-a7-postinc-false-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-a7-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-scc-a7-predec-false-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        set_tests_properties(raspi-arm32-qemu-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-loop-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=loop10"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bsr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsr4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cache-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_BOOTARGS=console=ttyAMA0 skip_reloc enable_cache;EMU68_QEMU_EXPECT_EXTRA=CACR=0x80008000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-disasm-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_BOOTARGS=console=ttyAMA0 skip_reloc disassemble;EMU68_QEMU_EXPECT_EXTRA=moveq   #$2a, d0"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-stack-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=stack42"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-a0mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=a0mem42"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-pea-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=pea_a0;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiz42;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-branch-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibeq7"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-dbf-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=dbf3;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000ffff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-dbne-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=dbne9;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-dbt-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=dbt7;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=snememff;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-true-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=stmemff;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-false-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=sfmem00;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-reg-sne-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=sneregff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-reg-sne-false-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=snereg00"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-reg-st-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=stregff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-reg-sf-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=sfreg00"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-trapf-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=trapf_w8"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-trapf-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=trapf_l9"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-trapf-none-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=trapf_n11"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movec-vbr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movecvbr15;EMU68_QEMU_EXPECT_EXTRA=VBR = 0x3f6fefb4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-moveusp-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=moveuspfb;EMU68_QEMU_EXPECT_EXTRA=USP= 0x3c100000;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movesr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movesr2000;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movetosr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movetosr2004;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000002ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-stop-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=stop2004;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000002ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bkpt-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bkptret11;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000013;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-illegal-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=illegalret12;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000014;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-rtd-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=rtdspfb;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-rtr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=rtrret05;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-trap3-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=trap3ret07;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000017;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-nop-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=nopret06"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-swap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=swap5678"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-lea-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=lea8a0"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-load-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemld12;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x55667788"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-base-load-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemlda0;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x55667788"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-store-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemst12;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x55667788"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-postinc-load-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movempost12;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x55667788;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100008"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-predec-store-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movempre12;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x55667788;EMU68_QEMU_EXPECT_POST=A0 = 0x3c0ffff8"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-predec-base-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemprea0;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x3c0ffff8;EMU68_QEMU_EXPECT_POST=A0 = 0x3c0ffff8"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-postinc-base-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemposta0;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x55667788;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100008"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemwd80;EMU68_QEMU_EXPECT_EXTRA=D3 = 0x00007fff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-word-base-load-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemwda0;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00007fff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-word-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemwpost23;EMU68_QEMU_EXPECT_EXTRA=D2 = 0xffff8001;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-word-postinc-base-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemwposta0;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00007fff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-word-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemwpre23;EMU68_QEMU_EXPECT_EXTRA=D2 = 0xffff8001;EMU68_QEMU_EXPECT_POST=A0 = 0x3c0ffffc"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movem-word-predec-base-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemwprea0;EMU68_QEMU_EXPECT_EXTRA=D2 = 0xfffffffc;EMU68_QEMU_EXPECT_POST=A0 = 0x3c0ffffc"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-ext-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=extwff80"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-ext-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=extl8034"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-extb-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=extbf80"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-ext-fused-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=extf80"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-link-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=linkwsp;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x3f7feffc"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-link-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=linklsp;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x3f7feffc"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-clr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=clrlz;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-not-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=notlfedc"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-neg-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=neglff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-tst-zero-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=tstlz"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-tst-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=tstbmi"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-tas-reg-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=tasreg81"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-tas-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=tasmem81"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-clr-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=clrbmem0"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-not-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=notbmemaa"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-neg-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=negbmemff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-tst-mem-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=tstbmemn"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsra04"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-unlk-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=unlka44;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-reset-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=resetok9"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-moveccr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=moveccr04;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000002ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-moves-store-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movesb_store33;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-moves-load-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movesb_load7f;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x1234567f;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-moves-load-addr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movesb_loada80;EMU68_QEMU_EXPECT_EXTRA=A1 = 0xffffff80;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-moves-postinc-a0-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movesb_posta0;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-moves-predec-a0-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movesb_prea0;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-moves-long-store-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movesl_storeef;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x89abcdef;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-moves-long-load-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movesl_load78;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x12345678;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-moves-word-load-addr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movesw_loada34;EMU68_QEMU_EXPECT_EXTRA=A1 = 0xffff8034;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-nbcd-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=nbcd96;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000011"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-nbcd-high-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=nbcd90h;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000011"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-nbcd-zero-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=nbcd0z;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=D2 = 0x00000004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-nbcd-x-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=nbcd99x;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000011"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-nbcd-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=nbcdpre96;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000011;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-abcd-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=abcd34;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-abcd-x-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=abcd00x;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x00000015"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-abcd-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=abcdpre1;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-exg-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=exgd01;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000011"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-exg-aa-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=exgaa01;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-exg-da-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=exgdafb;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpm-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sbcd-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=sbcd99x;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000099"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sbcd-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=sbcdpre99x;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addx-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addx01;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addx-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addxpre01;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subx-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subxff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subx-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subxpreff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-pack-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=packd1234"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-pack-add-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=packd1239"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-pack-sub-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=packd1231"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-unpk-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=unpkd1304"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-unpk-add-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=unpkd1309"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-unpk-sub-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=unpkd1301"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-trapt-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=trapt13;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-trapne-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=trapne14;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-trapv-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=trapv15;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-chk-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=chkok6"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-chk-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=chktrap12;EMU68_QEMU_EXPECT_EXTRA=VBR = 0x3f6fefc0"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movep-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movepw5678"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movep-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movepl12345678"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmp2ok11"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-chk2-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=chk2trap18"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-byte-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casb_ok21"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-byte-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casb_fail13;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casw_ok5678"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casl_ok89abcdef"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas2-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cas2w_ok2244;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00004444"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas2-long-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cas2l_fail3333;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x33333333"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-byte-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addqb79"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-byte-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subqb77"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addqw0000;EMU68_QEMU_EXPECT_EXTRA=SR = X.Z.C"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subqwffff;EMU68_QEMU_EXPECT_EXTRA=SR = XN..C"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-addr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addqa8;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100008"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-addr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subqa4;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c0ffffc"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addqb_postinc;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subqb_postinc;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addqb_predec;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subqb_predec;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-a7-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addqb_a7_postinc;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-a7-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subqb_a7_postinc;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-a7-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addqb_a7_predec;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-a7-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subqb_a7_predec;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-long-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addql_mem1;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-long-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subql_mem1;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-long-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addql_postinc;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-long-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subql_postinc;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-long-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addql_predec;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-long-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subql_predec;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-word-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addqw_mem1;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-word-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subqw_mem1;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-word-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addqw_postinc;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-word-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subqw_postinc;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-word-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addqw_predec;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-word-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subqw_predec;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addq-byte-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addqb_mem1;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subq-byte-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subqb_mem1;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-mem-false-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=snemem00;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=snepostff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-postinc-false-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=snepost00;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=snepreff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-predec-false-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=snepre00;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-a7-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=snea7postff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-a7-postinc-false-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=snea7post00;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-a7-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=snea7preff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-scc-a7-predec-false-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=snea7pre00;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    endif()
endif()

configure_file(include/version.h.in include/version.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/.git/index)
