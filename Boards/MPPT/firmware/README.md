# Helios MPPT firmware

Open [HeliosMPPT.ino](HeliosMPPT/HeliosMPPT.ino) in Arduino IDE 2. Keep [Config.h](HeliosMPPT/Config.h), [Control.h](HeliosMPPT/Control.h) and [PowerStage.h](HeliosMPPT/PowerStage.h) in the same folder. [COMPLETE_SOURCE.txt](COMPLETE_SOURCE.txt) contains all four files, separated by file markers. You can also [download the complete package](HeliosMPPT-full.zip).

## Verified build target

- STM32 MCU based boards 3.1.0
- Board: Generic STM32L0 series
- Board part number: Generic L072CZTx
- U(S)ART support: Enabled (no generic Serial)
- USB support: None
- Upload: STM32CubeProgrammer (SWD), through J4
- FQBN: STMicroelectronics:stm32:GenL0:pnum=GENERIC_L072CZTX,xserial=none,usb=none

Install the STM32 core using the [official Boards Manager index](https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json).

## Source files

| File | Purpose |
| --- | --- |
| `HeliosMPPT.ino` | Startup, ADC/DAC measurements, commands, load output, telemetry and watchdogs |
| `Config.h` | Battery profile, confirmation settings, calibration and PWM options |
| `Control.h` | Portable charging state machine and output-power P&O controller |
| `PowerStage.h` | TIM3 setup, bootstrap startup, compare updates and PWM stop |
| `tests/control_test.cpp` | Host tests of the controller and discrete timer model |

## Battery and configuration

This implementation has real TIM3 PWM, measurements, CC/CV regulation, output-power perturb-and-observe, charge termination/restart, load shedding, sample heartbeat and an independent watchdog.

The supplied reference profile is for a protected **3S Li-ion battery with a 4.20 V/cell maximum**, using a conservative 12.55 V charge target and 2 A converter-output limit. It is not an AGM or LiFePO4 profile. A BMS must provide independent cell protection and balancing. The connected DRIVER2 and its pumps receive the battery voltage, so their actual 10.5–12.6 V operating compatibility must be checked. This profile is an electrical reference, not a measurement of the installed battery.

In Config.h, the two confirmation macros default to 0:

```cpp
#define HELIOS_BATTERY_CONFIRMED 0
#define HELIOS_CALIBRATION_CONFIRMED 0
```

Set both to 1 only after verifying the actual battery matches this profile and calibrating the voltage/current measurements and TH1. A value of 0 leaves the controller in Unconfigured with the power stage off. With both confirmed and autoStart true, charging starts after three seconds of valid solar headroom and stable 5 V power-good.

Calibration fields include the measured voltage-divider corrections, shunt/gain, converter/load current zero corrections and the NTC beta. TH1 is only specified as 10 kOhm in the schematic; 3950 K is an explicit placeholder, not a verified part specification. Check zero current with loads disconnected and charging stopped, then check scaling against a known voltage/current.

## Control and observability

Inspect telemetry and command through SWD. Set command to 1 for START, 2 for STOP, 3 to CLEAR a latched fault. Clearing never starts the charger. Optional UART2 on PA2 TX/PA3 RX uses S, X and C; it requires separate access to those pins. The 8P8C link and USART1 are unused.

The charge states are Unconfigured, Stopped, Waiting, Qualifying, Charging, Complete and Fault. Unlike lead-acid charging, this Li-ion profile has no continuous float stage. Complete follows five minutes of battery current below 0.15 A near the voltage target; charging can restart below 12.15 V. Charge time is limited to six hours. Loads disconnect at 10.8 V and reconnect at 11.7 V. The XT30 rail is unswitched and cannot be disconnected by LOAD_EN.

TIM3 uses one center-aligned counter at nominal 40 kHz. CH3/PB0 uses PWM1 and CH4/PB1 PWM2 with a 500 ns compare gap, atomic preload updates and explicit off outputs. This uses compare separation because TIM3 has no advanced-timer complementary dead-time unit. Default synchronous=false keeps the low-side input off during normal freewheeling, apart from a 2 us bootstrap startup pulse. This reduces software-driven reverse-current risk but increases low-side body-diode losses. Optional synchronous mode requires measured current above the estimated half-ripple plus margin.

The 1 kHz TIM21 heartbeat stops PWM and the switched load after a missed 20 ms control deadline. IWDG resets the MCU after roughly 500 ms of a stalled loop. Both are software defenses, not a hardware cycle-by-cycle overcurrent comparator. A CPU reset does not establish the behavior of un-driven gate inputs before setup runs.

There is no panel-current sensor on this board. The MPPT perturbation maximizes measured converter output power as a proxy; input efficiency is not measured. Firmware tests do not verify switching transients, temperature, ADC noise, loop stability or actual dead time at MOSFET gates. Check those on the real board before a power test. The unchanged AP2210 and LM5109B supply limits still apply; raising the profile to 14.5 V does not solve them.

## Test results

Host C++: 25 checks passed, including a discrete counter model at 88 duty settings in both directions. Tests cover configuration, solar qualification, stop/clear, fault latching, stale/invalid readings, current/temperature/voltage trips, charge completion/recharge, timeout, voltage hold, load hysteresis and tick rollover. These are software tests, not physical-board tests.

To run the portable test with a C++17 compiler from tests/:

```sh
g++ -std=c++17 -Wall -Wextra -pedantic control_test.cpp -o control_test
./control_test
```

Arduino STM32 build results and memory use are recorded in [BUILD_RESULTS.txt](BUILD_RESULTS.txt). Both configurations compile without warnings. Physical programming, waveforms and charging still need checking on the board.
