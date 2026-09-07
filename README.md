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
loop in `Core/Src/main.c`. Use `HAL_Delay` to time the HIGH and LOW parts. You do
not need to implement timer PWM, interrupt handlers, or a separate driver file.
Keep the existing HAL initialization and tick running so `HAL_Delay` works.
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
void servo_pulse(GPIO_TypeDef *port, uint16_t pin, uint32_t high_time_ms);
```

| Parameter | Meaning | Example |
| --- | --- | --- |
| `port` | GPIO port containing the signal pin | `GPIOA` |
| `pin` | HAL pin mask, not a plain pin number | `GPIO_PIN_0` or `GPIO_PIN_1` |
| `high_time_ms` | Requested HIGH delay in whole milliseconds; use 1 or 2 for this exercise | `1` requests 1 ms |

Have the function produce one HIGH/LOW cycle whose **requested delays total
20 ms**, then return. Clamp `high_time_ms` to 1–2 before calculating the LOW delay.
Assume the port and pin are valid and already configured. The function blocks
while generating its frame and does not report whether the servo has physically
reached a position. Actual timing is approximate; see the HAL timing note below.

Call it repeatedly from the existing infinite loop in `main` to hold a position.
Alternatively, put the same sequence directly in that loop without a helper
function. A single call sends only one pulse; it does not maintain the signal.

### Using HAL_Delay

The HAL already provides this function; do not implement it yourself:

```c
void HAL_Delay(uint32_t Delay);
```

`Delay` is a **whole number of milliseconds**, not microseconds. For example,
`HAL_Delay(1)` requests a wait of at least 1 ms and `HAL_Delay(19)` requests at
least 19 ms. `HAL_Delay(1500)` waits about 1.5 seconds, not 1,500 µs.
Do not pass `0.5` or `1.5`: the integer parameter discards the fractional part.

Use it from the normal main loop after initialization. Leave interrupts enabled
and do not suspend the HAL tick. This project already uses TIM6 internally for
the HAL time base; you do not need to write timer code for the exercise.

### Set up the repeating loop

The servo's target period is **20 ms from the start of one HIGH pulse to the
start of the next**. Divide that period into a HIGH delay and a LOW delay:

**Requested LOW delay (ms) = 20 − requested HIGH delay (ms)**

| Requested HIGH delay | Requested LOW delay | Sum of requested delays | Nominal duty cycle |
| --- | --- | --- | --- |
| 1 ms | 19 ms | 20 ms | 5% |
| 2 ms | 18 ms | 20 ms | 10% |

Write these steps inside the existing infinite loop in `main`, or inside your
optional `servo_pulse` function called once per loop:

1. Choose a HIGH delay of 1 or 2 milliseconds.
2. Set the selected GPIO HIGH using `HAL_GPIO_WritePin`.
3. Call `HAL_Delay` with the chosen HIGH delay.
4. Set the GPIO LOW using `HAL_GPIO_WritePin`.
5. Call `HAL_Delay` with **20 minus the chosen HIGH delay**.
6. Repeat immediately. Do not add another delay after the frame.

A 20 ms LOW delay plus a 1 ms HIGH delay would request a 21 ms period, so the
LOW delay must account for time already spent HIGH. Keep other work out of this
loop while checking the waveform, since it adds time between pulses.

### What timing can this exercise achieve?

`HAL_Delay` is suitable for learning the sequence, but it cannot reproduce all
of the servo timings in the reference table. Its whole-millisecond arguments
cannot request 500, 1,500, or 2,500 µs precisely. Start with the 1 ms request;
this exercise does not provide precise angle control or the old valve calibration.

The bundled HAL also adds one tick to each requested delay to guarantee a minimum
wait. With the existing 1 ms tick, each call can take roughly one extra millisecond,
plus any execution overhead. Consequently, the 1 + 19 ms requests can produce a
frame closer to 22 ms, and the HIGH pulse can be longer than 1 ms. The table above
shows requested timing, not guaranteed measured timing. Measure both the HIGH
pulse and total period; do not assume that a 2 ms request stays below 2,500 µs.
Exact 20 ms frames and sub-millisecond pulse control are outside this simple
`HAL_Delay` exercise. Do not change the HAL implementation for this activity.

### Previous valve positions (optional reference)

These cannot be reproduced accurately with whole-millisecond `HAL_Delay` calls.
You do not need them to complete the exercise. They were calibrated for the old
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
3. Start with requested delays of 1 ms HIGH and 19 ms LOW. Measure the selected
   pin with an oscilloscope or logic analyzer and compare its actual HIGH time
   and period with the requests and the 20 ms target.
4. Change the requested HIGH delay and check that you also subtract it from the
   LOW delay. Measure the result before using it to command the servo.
5. Try an out-of-range argument and confirm that your function clamps the
   requested HIGH delay to 1–2 ms before calculating the remainder.
