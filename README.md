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

## Learning exercise: build a servo driver

Write a driver that controls a servo's position with hardware PWM. The data below
comes from the removed valve-board driver and the existing CubeMX configuration;
it describes the previous firmware settings, not a manufacturer datasheet.
No driver implementation is provided here.

### Signal and timer settings

The position is set by the duration of the HIGH pulse in each repeating frame.
Keep the frame period fixed while changing the pulse width.

| Parameter | Value |
| --- | --- |
| PWM period | 20,000 µs (20 ms) |
| PWM frequency | 50 Hz |
| Minimum HIGH pulse | 500 µs |
| Midpoint HIGH pulse | 1,500 µs |
| Maximum HIGH pulse | 2,500 µs |
| Duty-cycle range | 2.5–12.5% (midpoint: 7.5%) |
| Previous angle mapping | 0–270°, linear across 500–2,500 µs |
| Timer | TIM2, handle `htim2` |
| Timer input clock | 170 MHz with the existing clock configuration |
| Prescaler register (PSC) | 169 |
| Counter tick | 1 µs (1 MHz counter clock) |
| Auto-reload register (ARR) | 19,999 (20,000 ticks per frame) |
| PWM mode / polarity | PWM mode 1 / active HIGH |

| Output | Timer channel constant | MCU pin | GPIO alternate function |
| --- | --- | --- | --- |
| Dump servo | `TIM_CHANNEL_1` | PA0 | AF1 TIM2 |
| Normally-open (NO) servo | `TIM_CHANNEL_2` | PA1 | AF1 TIM2 |

CubeMX already configures these channels, but the starter application does not
start PWM. Both channels initially have a compare value of zero.

### Suggested function interface

Put declarations in `Drivers/Servo/servo_driver.h` and implement them in
`Drivers/Servo/servo_driver.c`. Include `main.h` for the STM32 HAL types and
`stdint.h` for the fixed-width integer types. These are declarations only:

```c
HAL_StatusTypeDef servo_start(TIM_HandleTypeDef *htim,
                             uint32_t channel,
                             uint16_t initial_pulse_us);

HAL_StatusTypeDef servo_set_pulse_us(TIM_HandleTypeDef *htim,
                                    uint32_t channel,
                                    uint16_t pulse_width_us);

uint16_t servo_degrees_to_us(uint16_t degrees);
```

| Parameter | Meaning / accepted values |
| --- | --- |
| `htim` | Pointer to the initialized PWM timer; use `&htim2` on this board. |
| `channel` | HAL channel constant: `TIM_CHANNEL_1` or `TIM_CHANNEL_2`, not a plain channel number. |
| `initial_pulse_us` | First HIGH pulse width, in microseconds; 500–2,500 inclusive. |
| `pulse_width_us` | Requested HIGH pulse width, in microseconds; 500–2,500 inclusive. |
| `degrees` | Absolute angle from 0 to 270; clamp larger values to 270. |

Suggested behavior:

- `servo_start`: validate the arguments, set the initial compare value, then start
  that PWM channel. Return the HAL start status, or `HAL_ERROR` for invalid inputs.
- `servo_set_pulse_us`: update the compare value of an already started channel.
  Return `HAL_OK` on success. Reject a null timer, an unsupported channel, or an
  out-of-range pulse with `HAL_ERROR`, leaving the output unchanged.
- `servo_degrees_to_us`: return the pulse width for the clamped absolute angle.
  Use integer arithmetic with a wide enough intermediate value and truncate the
  fractional microseconds to match the previous driver.

Starting PWM is a separate operation from changing position. Hardware keeps
producing pulses after a position update; the setter should not wait for the
servo to finish moving. These interfaces assume the fixed 20 ms timer setup above.

### Work out the conversion

Use these relationships to derive your implementation:

- Counter frequency = timer input clock ÷ (PSC + 1).
- Frame period = (ARR + 1) ÷ counter frequency.
- Duty cycle (%) = pulse width ÷ frame period × 100, using matching units.
- Compare value = pulse width in microseconds × (ARR + 1) ÷ 20,000.
- Pulse width in microseconds = 500 + absolute angle × (2,500 − 500) ÷ 270.

With this timer configuration, the compare value numerically equals the pulse
width in microseconds. Keep the angle calculation in at least 32-bit arithmetic
before converting the result to `uint16_t`.

### Previous valve calibration

The old driver added a per-servo zero offset to the requested relative angle,
clamped the result to 0–270°, then converted it to a pulse width. Its convention
was positive relative angles clockwise and negative angles counterclockwise.
These positions describe the old valve linkage; they are not generic endpoints
for every servo installation.

| Servo / state | Zero offset | Relative angle | Absolute angle | HIGH pulse | Duty cycle |
| --- | --- | --- | --- | --- | --- |
| NO open | 4° | 0° | 4° | 529 µs | 2.645% |
| NO closed | 4° | 90° | 94° | 1,196 µs | 5.980% |
| Dump open | 2° | 3° | 5° | 537 µs | 2.685% |
| Dump closed | 2° | 90° | 92° | 1,181 µs | 5.905% |

The old driver's comment mentioned 1,000 µs for open and 2,000 µs for closed;
its actual calibrated functions used the values in this table.

### Build and check your driver

1. Create the header and source files above. Look up the local HAL declarations
   for `HAL_TIM_PWM_Start` and `__HAL_TIM_SET_COMPARE` to work out how to start a
   channel and update its pulse width.
2. Add the source file to the executable with `target_sources` in the root
   `CMakeLists.txt`, and add `Drivers/Servo` with `target_include_directories`.
3. Include your header in a user section of `Core/Src/main.c`. Initialize your
   chosen servo channel after CubeMX's timer initialization, then request positions
   from the application. Choose the initial pulse explicitly.
4. Build using `./build.py build --debug`.
5. Check the PWM pin with an oscilloscope or logic analyzer. Confirm a 20 ms period
   and HIGH pulses of 500, 1,500, and 2,500 µs for absolute angles of 0°, 135°, and
   270°. Check the calibrated positions against the table as well.
6. Check that invalid pulse widths do not change the output, and that updating one
   channel leaves the other channel's compare value unchanged.
