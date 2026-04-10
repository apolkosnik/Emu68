cmake_minimum_required(VERSION 3.14.0)
project(Emu68 VERSION 1.0.7)
set(PROJECT_VERSION 1.0.7)
include(CTest)
include(CheckCCompilerFlag)

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
    check_c_compiler_flag(
        "-mbig-endian -mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4"
        EMU68_ARMHF_COMPILER_SUPPORTS_REQUIRED_FLAGS)
    if(NOT EMU68_ARMHF_COMPILER_SUPPORTS_REQUIRED_FLAGS)
        message(FATAL_ERROR
            "TARGET=raspi requires an ARM hard-float big-endian compiler. "
            "Reconfigure from a clean build directory with "
            "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/toolchains/arm-elf.cmake.")
    endif()

    set(ARMHF_TRANSLATOR_FILES
        src/armhf/M68k_Translator.c
        src/armhf/M68k_SR.c
        src/armhf/M68k_MULDIV.c
        src/armhf/M68k_MOVE.c
        src/armhf/M68k_EA.c
        src/armhf/M68k_LINE0.c
        src/armhf/M68k_LINE4.c
        src/armhf/M68k_LINE5.c
        src/armhf/M68k_LINE6.c
        src/armhf/M68k_LINE8.c
        src/armhf/M68k_LINE9.c
        src/armhf/M68k_LINEB.c
        src/armhf/M68k_LINEC.c
        src/armhf/M68k_LINED.c
        src/armhf/M68k_LINEE.c
        src/armhf/M68k_LINEF.c
        src/armhf/M68k_Exception.c
        src/armhf/M68k_CC.c
    )
    list(APPEND EMU68_FILES ${ARMHF_TRANSLATOR_FILES})
    set_source_files_properties(${ARMHF_TRANSLATOR_FILES} PROPERTIES COMPILE_FLAGS
        "-Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-function -Wno-overflow -Wno-shift-count-overflow")
    list(APPEND ARCH_FILES
        src/armhf/start.c
        src/armhf/mmu.c
        src/armhf/RegisterAllocator.c
        src/armhf/vectors.c
        src/armhf/ExecutionLoop.c
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
    set_source_files_properties(src/aarch64/ExecutionLoop.c PROPERTIES COMPILE_FLAGS 
        "-fcall-used-x4 -fcall-used-x5 -fcall-used-x6 -fcall-used-x7 -fcall-used-x8 \
         -fcall-used-x9 -fcall-used-x10 -fcall-used-x11 -ffixed-x12 -ffixed-x18  \
         -ffixed-x19 -ffixed-x20 -ffixed-x21 -ffixed-x22 -ffixed-x23 -ffixed-x24 -ffixed-x25 -ffixed-x26  \
         -ffixed-x13 -ffixed-x14 -ffixed-x15 -ffixed-x16 -ffixed-x17 -ffixed-x27 -ffixed-x28 -ffixed-x29")

    set(AARCH64_TRANSLATOR_FILES
        src/aarch64/M68k_Translator.c
        src/aarch64/M68k_SR.c
        src/aarch64/M68k_MULDIV.c
        src/aarch64/M68k_MOVE.c
        src/aarch64/M68k_EA.c
        src/aarch64/M68k_LINE0.c
        src/aarch64/M68k_LINE4.c
        src/aarch64/M68k_LINE5.c
        src/aarch64/M68k_LINE6.c
        src/aarch64/M68k_LINE8.c
        src/aarch64/M68k_LINE9.c
        src/aarch64/M68k_LINEB.c
        src/aarch64/M68k_LINEC.c
        src/aarch64/M68k_LINED.c
        src/aarch64/M68k_LINEE.c
        src/aarch64/M68k_LINEF.c
        src/aarch64/M68k_Exception.c
        src/aarch64/M68k_CC.c
    )

    list(APPEND ARCH_FILES
        src/aarch64/start.c
        src/aarch64/mmu.c
        src/aarch64/RegisterAllocator64.c
        src/aarch64/vectors.c
        src/aarch64/ExecutionLoop.c
    )
    list(APPEND EMU68_FILES ${AARCH64_TRANSLATOR_FILES})
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
            NAME raspi-arm32-qemu-swap-zero-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-swap-negative-smoke
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
            NAME raspi-arm32-qemu-movem-store-base-smoke
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
            NAME raspi-arm32-qemu-movem-word-store-base-smoke
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
            NAME raspi-arm32-qemu-negx-zero-smoke
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
            NAME raspi-arm32-qemu-negx-mem-smoke
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
            NAME raspi-arm32-qemu-jsr-disp-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-index-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-index-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-index-along-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-index-word-scale2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-index-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-pcdisp-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-pcindex-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-pcindex-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-pcindex-along-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-pcindex-word-scale2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-pcindex-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jsr-absl-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-disp-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-index-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-index-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-index-along-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-index-word-scale2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-index-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-pcdisp-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-pcindex-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-pcindex-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-pcindex-along-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-pcindex-word-scale2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-pcindex-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-jmp-absl-smoke
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
            NAME raspi-arm32-qemu-reset-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-moveccr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-ori-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-ori-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-andi-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-andi-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eori-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eori-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addi-byte-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addi-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addi-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addi-byte-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addi-word-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addi-long-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subi-byte-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subi-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subi-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subi-byte-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subi-word-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subi-long-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-ori-byte-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-andi-word-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eori-long-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addi-byte-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subi-byte-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-ori-byte-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-andi-word-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eori-long-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addi-byte-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subi-byte-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-ori-byte-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-andi-word-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eori-long-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addi-byte-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subi-word-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-ori-byte-a7-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-andi-byte-a7-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eori-byte-a7-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addi-byte-a7-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subi-byte-a7-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-a7-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-ori-byte-absl-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-andi-word-absl-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eori-long-absl-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-addi-byte-absl-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-subi-word-absl-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-absl-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcdisp-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcdisp-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcdisp-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcindex-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcindex-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcindex-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcindex-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcindex-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcindex-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcindex-windex-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcindex-windex-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcindex-windex-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcindex-along-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcindex-along-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcindex-along-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcindex-aword-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcindex-aword-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcindex-aword-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcindex-dlong-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcindex-dlong-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcindex-dlong-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcindex-aindex2-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcindex-aindex2-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcindex-aindex2-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcindex-windex-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcindex-windex-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcindex-windex-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcindex-aword-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcindex-aword-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcindex-aword-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcindex-dlong-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcindex-dlong-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcindex-dlong-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcindex-along-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcindex-along-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcindex-along-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-byte-pcindex-dlongx4-positive-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-word-pcindex-dlongx4-positive-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpi-long-pcindex-dlongx4-positive-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-ori-ccr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-andi-ccr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eori-ccr-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-imm-reg-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-reg-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-postinc-a7-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-postinc-a7-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-predec-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-predec-a7-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-predec-a7-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-disp-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-aindex2-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-lindex4-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-absl-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-absl-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-pcdisp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-pcdisp-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-pcindex-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-btst-pcindex-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-reg-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-postinc-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-disp-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-aindex2-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-lindex4-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-absl-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-absl-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-reg-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-postinc-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-disp-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-aindex2-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-lindex4-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-absl-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-absl-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-reg-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-postinc-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-predec-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-disp-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-aindex2-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-lindex4-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-absl-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-absl-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-postinc-a7-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-postinc-a7-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-postinc-a7-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-postinc-a7-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-postinc-a7-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-postinc-a7-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-predec-a7-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bchg-predec-a7-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-predec-a7-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bclr-predec-a7-mem-dyn-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-predec-a7-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-bset-predec-a7-mem-dyn-smoke
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
            NAME raspi-arm32-qemu-moves-trap-smoke
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
            NAME raspi-arm32-qemu-cmp-reg-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpa-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpm-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpm-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpm-word-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpm-long-negative-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpm-word-carry-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpm-long-carry-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpm-long-overflow-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpa-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpa-long-carry-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpa-long-overflow-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpa-word-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpa-long-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpa-long-mem-overflow-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmpa-long-mem-carry-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-reg-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-reg-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-carry-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-lindex4-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-lindex4-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-lindex4-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-reg-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-lindex4-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-reg-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-reg-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-reg-carry-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-reg-overflow-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-word-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-word-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-long-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-long-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-overflow-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-word-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp-mem-long-lindex4-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-reg-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-reg-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-word-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-word-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-long-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-long-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-word-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-eor-mem-long-lindex4-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-reg-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-reg-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-word-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-word-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-long-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-long-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-word-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-and-mem-long-lindex4-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-reg-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-reg-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-word-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-word-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-long-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-long-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-word-aindex2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-or-mem-long-lindex4-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-postinc-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-postinc-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-predec-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-predec-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-zero-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-zero-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-disp-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-disp-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-aindex2-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-aindex2-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-lindex4-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-lindex4-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-overflow-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-overflow-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-postinc-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-postinc-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-predec-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-predec-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-zero-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-long-zero-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-long-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-long-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-zero-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-rem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-overflow-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-overflow-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-zero-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-zero-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-postinc-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-postinc-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-predec-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-predec-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-disp-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-disp-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-aindex2-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-aindex2-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-lindex4-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-lindex4-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-zero-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-zero-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-postinc-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-postinc-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-predec-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-predec-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-postinc-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-postinc-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-predec-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-predec-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-disp-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-disp-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-aindex2-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-aindex2-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-lindex4-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-lindex4-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-disp-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-disp-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-aindex2-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-aindex2-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-long-64-lindex4-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-long-64-lindex4-zero-trap-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-long-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-long-postinc-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-long-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-long-predec-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-index-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-index-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-index-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-index-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-long-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-long-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-long-index-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-long-index-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-disp-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-disp-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-aindex2-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-aindex2-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divu-lindex4-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-divs-lindex4-overflow-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-long-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-long-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-mulu-long-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-muls-long-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-mem-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-mem-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-mem-long-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-mem-long-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-mem-long-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-mem-long-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-adda-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-suba-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-adda-mem-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-suba-mem-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-adda-mem-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-suba-mem-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-disp-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-disp-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-disp-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-disp-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-long-disp-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-long-disp-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-adda-word-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-suba-word-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-adda-long-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-suba-long-disp-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-disp-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-disp-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-disp-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-disp-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-long-disp-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-long-disp-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-index-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-index-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-index-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-index-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-long-index-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-long-index-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-adda-word-index-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-suba-word-index-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-adda-long-index-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-suba-long-index-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-index-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-index-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-index-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-index-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-long-index-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-long-index-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-aindex-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-aindex-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-adda-word-aindex-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-suba-word-aindex-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-aindex-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-aindex-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-long-lindex-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-long-lindex-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-adda-long-lindex-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-suba-long-lindex-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-long-lindex-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-long-lindex-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-aindex2-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-aindex2-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-adda-word-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-suba-word-aindex2-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-aindex2-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-aindex2-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-long-lindex4-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-long-lindex4-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-adda-long-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-suba-long-lindex4-mem-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-long-lindex4-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-long-lindex4-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-postinc-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-postinc-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-predec-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-predec-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-postinc-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-postinc-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-predec-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-predec-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-postinc-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-postinc-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-predec-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-predec-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-postinc-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-postinc-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-word-predec-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-word-predec-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-long-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-long-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-long-postinc-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-long-postinc-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-long-predec-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-long-predec-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-a7-postinc-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-a7-postinc-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-a7-predec-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-a7-predec-mem-dest-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-a7-postinc-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-a7-postinc-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-add-byte-a7-predec-mem-src-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-sub-byte-a7-predec-mem-src-smoke
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
            NAME raspi-arm32-qemu-chk-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-chk-long-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-chk-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-chk-negative-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movep-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movep-word-offset-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movep-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-movep-long-negative-offset-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp2-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp2-upper-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cmp2-lower-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-chk2-trap-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-chk2-lower-trap-smoke
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
            NAME raspi-arm32-qemu-cas-word-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-long-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-byte-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-byte-postinc-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-byte-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-byte-predec-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-word-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-word-postinc-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-word-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-word-predec-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-long-postinc-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-long-postinc-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-long-predec-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-long-predec-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-word-misaligned-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-word-misaligned-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-long-misaligned-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas-long-misaligned-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas2-word-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas2-word-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas2-word-first-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas2-long-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas2-long-fail-smoke
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-raspi32-smoke.sh ${CMAKE_BINARY_DIR}/Emu68.img
        )
        add_test(
            NAME raspi-arm32-qemu-cas2-long-first-fail-smoke
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
        set_tests_properties(raspi-arm32-qemu-swap-zero-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=swapz0;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-swap-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=swapn80;EMU68_QEMU_EXPECT_EXTRA=SR = .N..."
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
        set_tests_properties(raspi-arm32-qemu-movem-store-base-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemsta0;EMU68_QEMU_EXPECT_EXTRA=D3 = 0x55667788"
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
        set_tests_properties(raspi-arm32-qemu-movem-word-store-base-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movemwsta0;EMU68_QEMU_EXPECT_EXTRA=D3 = 0xffff8001"
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
        set_tests_properties(raspi-arm32-qemu-negx-zero-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=negx0z;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
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
        set_tests_properties(raspi-arm32-qemu-negx-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=negxbmemffx"
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
        set_tests_properties(raspi-arm32-qemu-jsr-disp-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsrdisp0b"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-index-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsridx17"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-index-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsridxa1x265"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-index-along-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsridxa1l75"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-index-word-scale2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsridxw255"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-index-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsridxneg45"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-pcdisp-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsrpc1d"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-pcindex-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsrpcidx21"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-pcindex-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsrpcidxa1x269"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-pcindex-along-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsrpcidxa1l79"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-pcindex-word-scale2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsrpcidxw259"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-pcindex-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsrpcidxneg49"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jsr-absl-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jsrabsl2d"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmpa0ret13"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-disp-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmpdisp0e"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-index-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmpidx1a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-index-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmpidxa1x26d"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-index-along-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmpidxa1l7b"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-index-word-scale2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmpidxw25d"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-index-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmpidxneg4d"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-pcdisp-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmppc24"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-pcindex-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmppcidx28"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-pcindex-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmppcidxa1x271"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-pcindex-along-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmppcidxa1l7d"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-pcindex-word-scale2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmppcidxw261"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-pcindex-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmppcidxneg51"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-jmp-absl-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=jmpabsl31"
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
        set_tests_properties(raspi-arm32-qemu-reset-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=resettrap10"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-moveccr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=moveccr04;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000002ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-ori-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=orib5a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-ori-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=oribmem5a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-andi-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andil2468"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-andi-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andiwmem50"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eori-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eoribff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eori-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eorilmemff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addi-byte-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addib7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addi-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addiw1234"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addi-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addil12345678"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addi-byte-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addibmem1"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addi-word-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addiwmem1"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addi-long-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addilmem1"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subi-byte-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subibff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subi-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subiwffff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subi-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subilffffffff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subi-byte-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subibmemff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subi-word-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subiwmemffff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subi-long-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subilmemffffffff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibmem2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwmem7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilmem7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-ori-byte-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=oribmemdisp5a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-andi-word-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andiwmemdisp50"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eori-long-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eorilmemdispff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addi-byte-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addibmemdisp1"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subi-byte-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subibmemdisp0"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibmemdisp2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-ori-byte-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=oribmemidxa1x25a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-andi-word-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andiwmemidxa1x250"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eori-long-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eorilmemlidx4ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addi-byte-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addibmemidxa1x21"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subi-byte-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subibmemlidx40"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibmemlidx42a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-ori-byte-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=oribmempost5a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-andi-word-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andiwmempre50"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eori-long-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eorilmempostff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addi-byte-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addibmempre1"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subi-word-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subiwmempost0"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibmempre2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-ori-byte-a7-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=oribmema7post5a;EMU68_QEMU_EXPECT_EXTRA=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-andi-byte-a7-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andibmema7pre50;EMU68_QEMU_EXPECT_EXTRA=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eori-byte-a7-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eoribmema7postff;EMU68_QEMU_EXPECT_EXTRA=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addi-byte-a7-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addibmema7pre1;EMU68_QEMU_EXPECT_EXTRA=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subi-byte-a7-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subibmema7post0;EMU68_QEMU_EXPECT_EXTRA=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-a7-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibmema7pre2a;EMU68_QEMU_EXPECT_EXTRA=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-ori-byte-absl-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=oribmemabsl5a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-andi-word-absl-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andiwmemabsl50"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eori-long-absl-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eorilmemabslff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-addi-byte-absl-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addibmemabsl1"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-subi-word-absl-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subiwmemabsl0"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-absl-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibmemabsl2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcdisp-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpc2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcdisp-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpc7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcdisp-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpc7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcindex-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpcidx2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcindex-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpcidx7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcindex-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpcidx7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcindex-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpcidxa1x22a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcindex-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpcidxa1x27f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcindex-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpcidxa1x27f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcindex-windex-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpcidxw2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcindex-windex-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpcidxw7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcindex-windex-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpcidxw7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcindex-along-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpcidxa1l2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcindex-along-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpcidxa1l7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcindex-along-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpcidxa1l7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcindex-aword-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpcidxa1w2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcindex-aword-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpcidxa1w7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcindex-aword-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpcidxa1w7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcindex-dlong-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpcidxdl2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcindex-dlong-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpcidxdl7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcindex-dlong-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpcidxdl7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcindex-aindex2-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpcidxa1x2neg2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcindex-aindex2-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpcidxa1x2neg7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcindex-aindex2-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpcidxa1x2neg7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcindex-windex-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpcidxwneg2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcindex-windex-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpcidxwneg7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcindex-windex-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpcidxwneg7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcindex-aword-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpcidxa1wneg2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcindex-aword-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpcidxa1wneg7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcindex-aword-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpcidxa1wneg7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcindex-dlong-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpcidxdlneg2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcindex-dlong-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpcidxdlneg7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcindex-dlong-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpcidxdlneg7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcindex-along-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpcidxa1lneg2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcindex-along-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpcidxa1lneg7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcindex-along-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpcidxa1lneg7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-byte-pcindex-dlongx4-positive-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpibpcidxdlx4p2a"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-word-pcindex-dlongx4-positive-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpiwpcidxdlx4p7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpi-long-pcindex-dlongx4-positive-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpilpcidxdlx4p7f"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-ori-ccr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=oriccr05;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-andi-ccr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andiccr0a;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eori-ccr-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eoriccr15;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-imm-reg-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstimmset;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-reg-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstdynmod32;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstmemff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstmemdynff;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstpostff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-postinc-a7-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstposta7ff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-postinc-a7-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstposta7dynff;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-predec-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstpredynff;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-predec-a7-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstprea7ff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-predec-a7-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstprea7dynff;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstdispff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-disp-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstdispdynff;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstidxa1x2ff;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-aindex2-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstidxa1x2dynff;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstidx4ff;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-lindex4-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstidx4dynff;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-absl-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstabslff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-absl-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstabsldynff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-pcdisp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstpcff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-pcdisp-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstpcdynff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffffff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-pcindex-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstpcidxff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffffff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-btst-pcindex-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=btstpcidxdynff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffffff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-reg-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgreg8;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgmem08;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgmemdyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-postinc-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgpostdyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgpre08;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgdisp08;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-disp-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgdispdyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgidxa1x208;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-aindex2-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgidxa1x2dyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgidx408;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-lindex4-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgidx4dyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-absl-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgabsl08;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-absl-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgabsldyn08;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-reg-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrreg0;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrmem7f;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrmemdyn7f;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-postinc-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrpostdyn7f;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrpre7f;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrdisp7f;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-disp-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrdispdyn7f;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclridxa1x27f;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-aindex2-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclridxa1x2dyn7f;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclridx47f;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-lindex4-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclridx4dyn7f;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-absl-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrabsl7f;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-absl-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrabsldyn7f;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-reg-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetreg8;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetmem08;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetmemdyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetpost08;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetpre08;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-postinc-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetpostdyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-predec-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetpredyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetdisp08;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-disp-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetdispdyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetidxa1x208;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-aindex2-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetidxa1x2dyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetidx408;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-lindex4-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetidx4dyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-absl-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetabsl08;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-absl-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetabsldyn08;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-postinc-a7-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgposta708;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-postinc-a7-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgposta7dyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-postinc-a7-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrposta77f;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-postinc-a7-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrposta7dyn7f;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-postinc-a7-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetposta708;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-postinc-a7-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetposta7dyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-predec-a7-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgprea708;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bchg-predec-a7-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bchgprea7dyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-predec-a7-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrprea77f;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bclr-predec-a7-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bclrprea7dyn7f;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-predec-a7-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetprea708;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-bset-predec-a7-mem-dyn-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=bsetprea7dyn08;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
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
        set_tests_properties(raspi-arm32-qemu-moves-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movestrap16"
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
        set_tests_properties(raspi-arm32-qemu-cmp-reg-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpregff;EMU68_QEMU_EXPECT_EXTRA=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpa-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpaeqff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpm-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmwff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpm-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmlff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpm-word-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmwneg;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpm-long-negative-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmlneg;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpm-word-carry-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmwcsff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpm-long-carry-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmlcsff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpm-long-overflow-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmlvsff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100004;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100008"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpa-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpawneg;EMU68_QEMU_EXPECT_EXTRA=A1 = 0xffff8000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpa-long-carry-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpalcsff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x00000001;EMU68_QEMU_EXPECT_POST=A1 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpa-long-overflow-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpalvsff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0xffffffff;EMU68_QEMU_EXPECT_POST=A1 = 0x7fffffff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpa-word-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpawmemneg;EMU68_QEMU_EXPECT_EXTRA=A1 = 0xffff8000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpa-long-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpalmemff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x12345678;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpa-long-mem-overflow-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpalmemvsff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x7fffffff;EMU68_QEMU_EXPECT_POST=SR = .N.VC"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmpa-long-mem-carry-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpalmemcsff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-reg-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eorregff;EMU68_QEMU_EXPECT_EXTRA=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-reg-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andreg50;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000050"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmempostff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100001;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmempreff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-carry-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemcsff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemidxa1x2ff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-lindex4-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemidx4ff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormem00;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffffff;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormempost00;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100001;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormempre00;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormemidxa1x2ff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-lindex4-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormemidx4ff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmem50;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000050;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmempost50;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100001;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmempre50;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmemidxa1x250;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-lindex4-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmemidx450;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-reg-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=orreg55;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000005"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormem55;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000055;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormempost55;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100001;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormempre55;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormemidxa1x255;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-lindex4-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormemidx455;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-reg-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpregwff;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-reg-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpreglff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x12345678;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-reg-carry-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpcsff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-reg-overflow-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpvsff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0x7fffffff;EMU68_QEMU_EXPECT_POST=D1 = 0xffffffff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemwff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-word-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemwpostff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100002;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-word-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemwpreff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemlff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-long-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemlpostff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-long-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemlpreff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-overflow-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemlvsff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=D0 = 0x7fffffff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-word-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemwidxa1x2ff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp-mem-long-lindex4-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmpmemlidx4ff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-reg-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eorregwff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-reg-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eorreglff;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormemwff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-word-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormemwpostff;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100002;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-word-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormemwpreff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormemlff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-long-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormemlpostff;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-long-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormemlpreff;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-word-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormemwidxa1x2ff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-eor-mem-long-lindex4-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=eormemlidx4ff;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-reg-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andregw50;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000050"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-reg-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andregl50;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffff0050"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmemw50;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-word-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmemwpost50;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100002;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-word-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmemwpre50;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmeml50;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-long-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmemlpost50;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-long-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmemlpre50;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-word-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmemwidxa1x250;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-and-mem-long-lindex4-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=andmemlidx450;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-reg-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=orregw55;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000005"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-reg-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=orregl55;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x12000005"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormemw55;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-word-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormemwpost55;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100002;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-word-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormemwpre55;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormeml55;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-long-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormemlpost55;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-long-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormemlpre55;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-word-aindex2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormemwidxa1x255;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-or-mem-long-lindex4-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=ormemlidx455;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divu5;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsfffb;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000002;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumem5;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemfffb;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000002;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumempost5;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmempostfffb;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumempre5;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemprefffb;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-postinc-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumempostztrap51;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000061;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-postinc-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmempostztrap53;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000063;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-predec-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumempreztrap55;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000065;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-predec-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmempreztrap57;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000067;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-zero-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divuztrap11;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000021;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc0"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-zero-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsztrap13;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000023;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc0"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-disp-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumemdispztrap31;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000041;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-disp-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemdispztrap33;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000043;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-aindex2-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumemidxa1x2ztrap35;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000045;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc8"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-aindex2-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemidxa1x2ztrap37;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000047;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc8"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-lindex4-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumemlidx4ztrap39;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000049;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc8"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-lindex4-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemlidx4ztrap3b;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000004b;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc8"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-overflow-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divuovf10000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0x00010000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-overflow-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsovf8000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0xffff8000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-postinc-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumempostovf10000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0x00010000;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-postinc-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmempostovf8000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0xffff8000;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-predec-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumempreovf10000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0x00010000;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-predec-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmempreovf8000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0xffff8000;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumemovf10000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0x00010000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemovf8000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0xffff8000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulu42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000007"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-zero-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=muluz0;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulsm42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000007;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulumem42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000007"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulsmem42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000007;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulumempost42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulsmempost42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulumempre42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulsmempre42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulul42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000007"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-long-zero-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mululz0;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x000000ff;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulsl42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000007;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-long-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mululmem42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000007"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-long-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulslmem42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000007;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-zero-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulz0;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000002a;EMU68_QEMU_EXPECT_POST=SR = ..Z.."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslfffa;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000007;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-rem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul42r1;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=D2 = 0x00000002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulmem42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslmemfffa;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000007;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulmempost42;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslmempostfffa;EMU68_QEMU_EXPECT_EXTRA=SR = .N...;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulmempre42;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslmemprefffa;EMU68_QEMU_EXPECT_EXTRA=SR = .N...;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulmemdisp42;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslmemdispfffa;EMU68_QEMU_EXPECT_EXTRA=SR = .N...;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulmemidxa1x242;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslmemidxa1x2fffa;EMU68_QEMU_EXPECT_EXTRA=SR = .N...;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulmemlidx442;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslmemlidx4fffa;EMU68_QEMU_EXPECT_EXTRA=SR = .N...;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64q10000000;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64neg3rneg1;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffffff;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64memq10000000;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64memneg3rneg1;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffffff;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-overflow-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64ovf100000000;EMU68_QEMU_EXPECT_RESULT=D3 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=D0 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-overflow-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64ovf80000000;EMU68_QEMU_EXPECT_RESULT=D3 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=D0 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64memovf100000000;EMU68_QEMU_EXPECT_RESULT=D3 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=D0 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64memovf80000000;EMU68_QEMU_EXPECT_RESULT=D3 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=D0 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-zero-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulztrap71;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000061;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc2"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-zero-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslztrap73;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000063;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc2"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulmemztrap75;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000065;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslmemztrap77;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000067;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-postinc-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulmempostztrap19;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000029;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-postinc-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslmempostztrap1b;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000002b;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-predec-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulmempreztrap1d;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000002d;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-predec-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslmempreztrap1f;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000002f;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-disp-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulmemdispztrap21;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000031;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-disp-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslmemdispztrap23;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000033;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-aindex2-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulmemidxa1x2ztrap25;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000035;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefa4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-aindex2-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslmemidxa1x2ztrap27;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000037;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefa4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-lindex4-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divulmemlidx4ztrap29;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000039;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefa4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-lindex4-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divslmemlidx4ztrap2b;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000003b;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefa4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-zero-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64ztrap79;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000069;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc2"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-zero-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64ztrap7b;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000006b;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc2"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64memztrap7d;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000006d;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64memztrap7f;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000006f;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64mempostq10000000;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000000;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64mempostneg3rneg1;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffffff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64mempreq10000000;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000000;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64mempreneg3rneg1;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffffff;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-postinc-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64mempostovf100000000;EMU68_QEMU_EXPECT_RESULT=D3 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-postinc-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64mempostovf80000000;EMU68_QEMU_EXPECT_RESULT=D3 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-predec-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64mempreovf100000000;EMU68_QEMU_EXPECT_RESULT=D3 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-predec-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64mempreovf80000000;EMU68_QEMU_EXPECT_RESULT=D3 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-postinc-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64mempostztrap49;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000049;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-postinc-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64mempostztrap4b;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000004b;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-predec-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64mempreztrap4d;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000004d;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-predec-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64mempreztrap4f;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x0000004f;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64memdispq10000000;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64memdispneg3rneg1;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffffff;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64memidxa1x2q10000000;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000000;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64memidxa1x2neg3rneg1;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffffff;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64memlidx4q10000000;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64memlidx4neg3rneg1;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffffff;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-disp-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64memdispovf100000000;EMU68_QEMU_EXPECT_RESULT=D4 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-disp-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64memdispovf80000000;EMU68_QEMU_EXPECT_RESULT=D4 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-aindex2-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64memidxa1x2ovf100000000;EMU68_QEMU_EXPECT_RESULT=D4 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-aindex2-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64memidxa1x2ovf80000000;EMU68_QEMU_EXPECT_RESULT=D4 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-lindex4-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64memlidx4ovf100000000;EMU68_QEMU_EXPECT_RESULT=D4 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-lindex4-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64memlidx4ovf80000000;EMU68_QEMU_EXPECT_RESULT=D4 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000001;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-disp-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64memdispztrap81;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffff81;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-disp-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64memdispztrap83;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffff83;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefc4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-aindex2-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64memidxa1x2ztrap85;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffff85;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefa4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-aindex2-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64memidxa1x2ztrap87;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffff87;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefa4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-long-64-lindex4-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divul64memlidx4ztrap89;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffff89;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefa4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-long-64-lindex4-zero-trap-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsl64memlidx4ztrap8b;EMU68_QEMU_EXPECT_EXTRA=D1 = 0xffffff8b;EMU68_QEMU_EXPECT_POST=VBR = 0x3f6fefa4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-long-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mululmempost42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-long-postinc-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulslmempost42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-long-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mululmempre42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-long-predec-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulslmempre42;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumemdisp5;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemdispfffb;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-index-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumemidx5;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-index-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemidxfffb;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulumemdisp42;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulsmemdisp42;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-index-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulumemidx42;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-index-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulsmemidx42;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-long-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mululmemdisp42;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-long-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulslmemdisp42;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-long-index-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mululmemidx42;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-long-index-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulslmemidx42;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumemidxa1x25;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemidxa1x2fffb;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumemlidx4x5;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemlidx4xfffb;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-disp-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumemdispovf10000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0x00010000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-disp-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemdispovf8000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0xffff8000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-aindex2-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumemidxa1x2ovf10000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0x00010000;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-aindex2-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemidxa1x2ovf8000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0xffff8000;EMU68_QEMU_EXPECT_POST=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divu-lindex4-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divumemlidx4ovf10000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0x00010000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-divs-lindex4-overflow-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=divsmemlidx4ovf8000;EMU68_QEMU_EXPECT_RESULT=D1 = 0xffffffff;EMU68_QEMU_EXPECT_EXTRA=D0 = 0xffff8000;EMU68_QEMU_EXPECT_POST=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulumemidxa1x242;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulsmemidxa1x242;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulumemlidx442;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulsmemlidx442;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-long-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mululmemidxa1x242;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-long-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulslmemidxa1x242;EMU68_QEMU_EXPECT_EXTRA=A2 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = .N..."
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-mulu-long-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mululmemlidx442;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-muls-long-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=mulslmemlidx442;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=SR = .N..."
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
        set_tests_properties(raspi-arm32-qemu-add-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addl3;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subl3;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00000002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-mem-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addlmem3;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-mem-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=sublmem3;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-mem-long-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addlpost3;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-mem-long-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=sublpost3;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-mem-long-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addlpre3;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-mem-long-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=sublpre3;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-adda-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addaw4;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-suba-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subaw4;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c0ffffc"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-adda-mem-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addawmemneg;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c0ffffc"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-suba-mem-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subawmemneg;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-adda-mem-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addalmem4;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-suba-mem-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subalmem4;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c0ffffc"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-disp-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbsrcdisp1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-disp-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbsrcdisp1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-disp-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwsrcdisp1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-disp-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwsrcdisp1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-long-disp-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addlsrcdisp3;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-long-disp-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=sublsrcdisp3;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-adda-word-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addawdispneg"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-suba-word-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subawdispneg"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-adda-long-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addaldisp4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-suba-long-disp-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subaldisp4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-disp-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbdstdisp1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-disp-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbdstdisp1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-disp-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwdstdisp1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-disp-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwdstdisp1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-long-disp-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addldstdisp1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-long-disp-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subldstdisp1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-index-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbsrcidx1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-index-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbsrcidx1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-index-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwsrcidx1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-index-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwsrcidx1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-long-index-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addlsrcidx3;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-long-index-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=sublsrcidx3;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-adda-word-index-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addawidxneg"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-suba-word-index-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subawidxneg"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-adda-long-index-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addalidx4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-suba-long-index-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subalidx4"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-index-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbdstidx1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-index-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbdstidx1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-index-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwdstidx1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-index-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwdstidx1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-long-index-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addldstidx1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-long-index-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subldstidx1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-aindex-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwsrcidxa1;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-aindex-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwsrcidxa1;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-adda-word-aindex-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addawidxa1neg;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-suba-word-aindex-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subawidxa1neg;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-aindex-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwdstidxa1;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-aindex-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwdstidxa1;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-long-lindex-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addlsrcidxl3;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c110004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-long-lindex-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=sublsrcidxl3;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c110004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-adda-long-lindex-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addalidxl4;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c110004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-suba-long-lindex-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subalidxl4;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c110004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-long-lindex-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addldstidxl1;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c110004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-long-lindex-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subldstidxl1;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c110004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-aindex2-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwsrcidxa1x2;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-aindex2-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwsrcidxa1x2;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-adda-word-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addawidxa1x2neg;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-suba-word-aindex2-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subawidxa1x2neg;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-aindex2-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwdstidxa1x2;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-aindex2-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwdstidxa1x2;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x00000001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-long-lindex4-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addlsrcidxl3x4;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-long-lindex4-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=sublsrcidxl3x4;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-adda-long-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addalidx4x4;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-suba-long-lindex4-mem-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subalidx4x4;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-long-lindex4-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addldstidx1x4;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-long-lindex4-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subldstidx1x4;EMU68_QEMU_EXPECT_EXTRA=A1 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbsrc1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbsrc1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-postinc-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbsrcpost1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-postinc-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbsrcpost1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-predec-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbsrcpre1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-predec-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbsrcpre1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwsrc1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwsrc1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-postinc-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwsrcpost1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-postinc-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwsrcpost1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-predec-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwsrcpre1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-predec-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwsrcpre1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbdst1"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbdstfe"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-postinc-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbdstpost1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-postinc-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbdstpostfe;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100001"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-predec-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbdstpre1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-predec-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbdstprefe;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwdst1"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwdst1"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-postinc-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwdstpost1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-postinc-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwdstpost1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100002"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-word-predec-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addwdstpre1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-word-predec-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subwdstpre1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-long-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addldst1"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-long-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subldst1"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-long-postinc-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addldstpost1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-long-postinc-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subldstpost1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100004"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-long-predec-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addldstpre1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-long-predec-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subldstpre1;EMU68_QEMU_EXPECT_EXTRA=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-a7-postinc-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbdsta7post1;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-a7-postinc-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbdsta7postfe;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-a7-predec-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbdsta7pre1;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-a7-predec-mem-dest-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbdsta7prefe;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-a7-postinc-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbsrca7post1;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-a7-postinc-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbsrca7postff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-add-byte-a7-predec-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=addbsrca7pre1;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-sub-byte-a7-predec-mem-src-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=subbsrca7preff;EMU68_QEMU_EXPECT_POST=A7 = 0x3f7ff000"
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
        set_tests_properties(raspi-arm32-qemu-chk-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=chklok9"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-chk-long-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=chkltrap18;EMU68_QEMU_EXPECT_EXTRA=VBR = 0x3f6fefc0"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-chk-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=chktrap12;EMU68_QEMU_EXPECT_EXTRA=VBR = 0x3f6fefc0"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-chk-negative-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=chkneg14;EMU68_QEMU_EXPECT_EXTRA=VBR = 0x3f6fefc0"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movep-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movepw5678"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movep-word-offset-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movepw_off5678;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movep-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movepl12345678"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-movep-long-negative-offset-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=movepl_neg12345678;EMU68_QEMU_EXPECT_POST=A0 = 0x3c100000"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp2-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmp2ok11"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp2-upper-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmp2high15"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cmp2-lower-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cmp2low16"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-chk2-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=chk2trap18"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-chk2-lower-trap-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=chk2low19"
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
        set_tests_properties(raspi-arm32-qemu-cas-word-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casw_fail1357;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x00001357"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casl_ok89abcdef"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-long-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casl_fail13579bdf;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x13579bdf"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-byte-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casb_post_ok21;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-byte-postinc-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casb_post_fail13;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100001;EMU68_QEMU_EXPECT_POST=D2 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-byte-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casb_pre_ok21;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-byte-predec-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casb_pre_fail13;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100000;EMU68_QEMU_EXPECT_POST=D2 = 0x000000ff"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-word-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casw_post_ok5678;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x00005678"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-word-postinc-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casw_post_fail1357;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100002;EMU68_QEMU_EXPECT_POST=D2 = 0x00001357"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-word-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casw_pre_ok5678;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x00005678"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-word-predec-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casw_pre_fail1357;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100000;EMU68_QEMU_EXPECT_POST=D2 = 0x00001357"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-long-postinc-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casl_post_ok89abcdef;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x89abcdef"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-long-postinc-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casl_post_fail13579bdf;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100004;EMU68_QEMU_EXPECT_POST=D2 = 0x13579bdf"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-long-predec-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casl_pre_ok89abcdef;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x89abcdef"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-long-predec-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casl_pre_fail13579bdf;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100000;EMU68_QEMU_EXPECT_POST=D2 = 0x13579bdf"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-word-misaligned-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casw_mis_ok5678;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x00005678"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-word-misaligned-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casw_mis_fail1357;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100001;EMU68_QEMU_EXPECT_POST=D2 = 0x00001357"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-long-misaligned-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casl_mis_ok89abcdef;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x89abcdef"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas-long-misaligned-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=casl_mis_fail13579bdf;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x3c100002;EMU68_QEMU_EXPECT_POST=D2 = 0x13579bdf"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas2-word-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cas2w_ok2244;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00004444"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas2-word-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cas2w_fail3333;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x00003333"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas2-word-first-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cas2w_fail5555;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x00003333;EMU68_QEMU_EXPECT_POST=D1 = 0x00005555"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas2-long-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cas2l_ok22444444;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x44444444"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas2-long-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cas2l_fail3333;EMU68_QEMU_EXPECT_EXTRA=D1 = 0x33333333"
            TIMEOUT 20
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(raspi-arm32-qemu-cas2-long-first-fail-smoke PROPERTIES
            ENVIRONMENT "EMU68_QEMU_TIMEOUT=8;EMU68_QEMU_PAYLOAD=cas2l_fail5555;EMU68_QEMU_EXPECT_EXTRA=D2 = 0x33333333;EMU68_QEMU_EXPECT_POST=D1 = 0x55555555"
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
