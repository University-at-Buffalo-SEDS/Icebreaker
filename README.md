# Icebreaker

STM32G491 starter project with CubeMX peripheral initialization and an empty main loop.
The STM32 HAL/CMSIS, startup code, and existing board pin configuration are retained.
Custom device drivers, application threads, networking, telemetry, USB middleware,
and OTA/bootloader integration have been removed.

## Build

Install CMake 3.22+, Ninja, and the Arm GNU embedded toolchain on PATH, then run:

```sh
cmake --preset Debug
cmake --build --preset Debug
```

Use the Release presets for a release build. The output is
`build/Debug/Icebreaker.elf`. Flash the ELF with an STM32 programming tool.
The application starts at `0x08000000` and uses the full 512 KiB flash region.

Add application code in the user sections of `Core/Src/main.c`.
`icebreaker.ioc` retains the original board hardware configuration for CubeMX.
