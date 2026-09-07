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

Alternatively, use the build and flash helper:

```sh
./build.py build --debug
./build.py build --release
./build.py flash --debug --method st-flash
```

The helper writes ELF and BIN files to `build/Debug_Script` or
`build/Release_Script` and flashes at `0x08000000` by default.

## Learning exercise: control a servo from main

Make a servo hold a position by turning a GPIO pin HIGH and LOW in a repeating
loop in `Core/Src/main.c`. Use software delays to time the pulses. You do not need
timer PWM, interrupts, a separate driver file, or another peripheral beyond GPIO.
The implementation is left for you to write.

### What signal does the servo need?

Repeat one pulse every **20 milliseconds** (50 pulses per second). The amount of
time the signal stays HIGH tells the servo which position to hold. Keep sending
pulses for as long as you want to command that position.

| Position | Time HIGH | Time LOW | Total period | Duty cycle |
| --- | --- | --- | --- | --- |
| 0° | 500 µs | 19,500 µs | 20,000 µs | 2.5% |
| 135° (midpoint) | 1,500 µs | 18,500 µs | 20,000 µs | 7.5% |
| 270° | 2,500 µs | 17,500 µs | 20,000 µs | 12.5% |

These are the settings used by the previous firmware, not independently verified
manufacturer specifications. Its angle range was 0–270° with a linear mapping
between 500 and 2,500 µs.

- 1 millisecond (ms) = 1,000 microseconds (µs).
- Time LOW = 20,000 µs − time HIGH.
- Duty cycle is the percentage of each period spent HIGH.
- A 20 ms LOW delay after the HIGH pulse would make the period too long: subtract
  the HIGH time from the period first.

### Choose and configure a pin

| Previous servo connection | MCU pin |
| --- | --- |
| Dump servo signal | PA0 |
| Normally-open (NO) servo signal | PA1 |

Choose one servo for the exercise. The existing project configures PA0 and PA1
as timer alternate-function pins. **Change your chosen pin to a normal push-pull
GPIO output, initially LOW**, before trying to toggle it. You can change this in
CubeMX or configure the GPIO in a user section of `main` after all generated
initialization calls. If configuring it in `main`, do so after `MX_TIM2_Init()`,
which otherwise restores the timer pin configuration. Do not start timer PWM.

Use `HAL_GPIO_WritePin` to set the output HIGH or LOW. The servo signal and board
need a shared ground; the GPIO is the control signal, not the servo's power supply.

### Suggested function

Keep the declaration and your implementation in user sections of
`Core/Src/main.c`. No extra source files or CMake changes are needed.
This is a prototype only:

```c
void servo_pulse(GPIO_TypeDef *port, uint16_t pin, uint16_t pulse_width_us);
```

| Parameter | Meaning | Example |
| --- | --- | --- |
| `port` | GPIO port containing the signal pin | `GPIOA` |
| `pin` | HAL pin mask, not a plain pin number | `GPIO_PIN_0` or `GPIO_PIN_1` |
| `pulse_width_us` | Requested HIGH time, 500–2,500 µs inclusive | `1500` for midpoint |

Have the function produce **one complete 20 ms frame** and then return. Clamp
pulse widths below 500 µs to 500 µs and above 2,500 µs to 2,500 µs. Assume the port
and pin are valid and already configured. The function blocks while generating
its frame and does not report whether the servo has physically reached a position.

Call it repeatedly from the existing infinite loop in `main` to hold a position.
Alternatively, put the same sequence directly in that loop without a helper
function. A single call sends only one pulse; it does not maintain the signal.

### Work out the loop

For each frame:

1. Choose a HIGH time within the allowed range.
2. Set the signal pin HIGH.
3. Wait for the chosen number of microseconds.
4. Set the signal pin LOW.
5. Wait for the remainder of the 20,000 µs period.
6. Repeat.

For this exercise, write a software busy-wait delay using a loop. You can give it
this interface, also in `main.c`:

```c
void delay_us(uint32_t microseconds);
```

A loop iteration is **not automatically one microsecond**. Its timing depends on
the CPU clock, compiler optimization, and instructions inside the loop. An empty
loop may be optimized away entirely. Work out how to keep the delay loop present
and calibrate it using an oscilloscope or logic analyzer. Recheck it after changing
build settings. The current project runs the CPU at 170 MHz, but that does not
mean a C loop takes exactly one clock cycle per iteration.

Do not use `HAL_Delay` for this exercise: it depends on a timer-backed HAL tick
and accepts milliseconds, which is too coarse for these pulse widths. Software
loop timing is approximate; GPIO calls, loop overhead, and interrupts can extend
the measured pulse or period. Use the measured waveform to adjust your delay.
This approach is a learning exercise and occupies the CPU while controlling the
servo. Precise timing and doing other work at the same time are later topics.

### Optional: choose a position in degrees

Once a fixed pulse works, convert an absolute angle to a pulse width:

**Pulse width (µs) = 500 + angle × 2,000 ÷ 270**

Clamp the angle to 0–270° first. Use at least 32-bit arithmetic for the
multiplication and truncate fractional microseconds to match the previous driver.
You can do this calculation directly in `main`.

### Previous valve positions (optional reference)

You do not need these to complete the exercise. They were calibrated for the old
valve linkage and are not generic open/closed positions for every installation.

| Servo / state | Absolute angle | HIGH time | LOW time | Duty cycle |
| --- | --- | --- | --- | --- |
| NO open | 4° | 529 µs | 19,471 µs | 2.645% |
| NO closed | 94° | 1,196 µs | 18,804 µs | 5.980% |
| Dump open | 5° | 537 µs | 19,463 µs | 2.685% |
| Dump closed | 92° | 1,181 µs | 18,819 µs | 5.905% |

### Build and check

1. Write your loop or functions in the user sections of `Core/Src/main.c`.
2. Build with `./build.py build --debug` and flash using your board's programmer.
3. Measure your selected pin. Start with a 1,500 µs HIGH pulse and a total period
   of approximately 20 ms, repeated continuously.
4. Change the requested pulse width and check that the HIGH time changes while
   the total period remains approximately 20 ms.
5. Try an out-of-range request and confirm that your clamping keeps the HIGH time
   within 500–2,500 µs.
