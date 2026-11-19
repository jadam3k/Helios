#pragma once
#include <stdint.h>
#ifndef HELIOS_BATTERY_CONFIRMED
#define HELIOS_BATTERY_CONFIRMED 0
#endif
#ifndef HELIOS_CALIBRATION_CONFIRMED
#define HELIOS_CALIBRATION_CONFIRMED 0
#endif
namespace cfg {
// Charging requires an identified battery and measured sensor calibration.
// Set these to true ONLY after checking the corresponding real hardware.
constexpr bool batteryConfirmed = HELIOS_BATTERY_CONFIRMED != 0;
constexpr bool calibrationConfirmed = HELIOS_CALIBRATION_CONFIRMED != 0;
constexpr bool autoStart = true;
constexpr bool uartTelemetry = false; // PA2 TX / PA3 RX, not RJ45.
// Reference for the UNCHANGED board: protected 3S Li-ion, 4.20 V/cell maximum.
// This profile is NOT for 4S LiFePO4, 12 V AGM, or 3S LiFePO4.
constexpr float chargeVoltage = 12.55f;
constexpr float batteryTripVoltage = 12.75f;
constexpr float batteryMinimum = 10.5f;
constexpr float rechargeVoltage = 12.15f;
constexpr float maxConverterA = 2.0f;
constexpr float tripConverterA = 2.5f;
constexpr float tripLoadA = 5.0f; // Software threshold; not a board current rating.
constexpr float terminationA = 0.15f;
constexpr float panelMaximum = 28.0f; // Verify capacitor ratings and cold Voc.
constexpr float startHeadroom = 2.0f;
constexpr float stopHeadroom = 0.6f;
constexpr float loadOffVoltage = 10.8f;
constexpr float loadOnVoltage = 11.7f;
constexpr float minimumC = 5.0f, maximumC = 40.0f;
constexpr uint32_t qualificationMs = 3000, terminationMs = 300000;
constexpr uint32_t chargeTimeoutMs = 6UL * 60 * 60 * 1000;
constexpr uint32_t pwmHz = 40000, deadTimeNs = 500;
constexpr bool synchronous = false; // Default: low-side body-diode freewheel.
constexpr float inductanceH = 47e-6f;
constexpr float vddaCalibration = 1.0f;
constexpr float voltageRatio = 105600.0f / 5600.0f;
constexpr float batteryVoltageCalibration = 1.0f, panelVoltageCalibration = 1.0f;
constexpr float shuntOhm = 0.003f, currentGain = 20.0f;
constexpr float converterZeroCorrectionV = 0.0f, loadZeroCorrectionV = 0.0f;
constexpr float ntcR25 = 10000.0f, ntcPullup = 10000.0f;
constexpr float ntcBeta = 3950.0f; // TH1 unspecified: replace with actual data.
constexpr float dutyMinimum = 0.03f, dutyMaximum = 0.90f;
static_assert(chargeVoltage < batteryTripVoltage, "Charge/trip voltage order");
static_assert(batteryTripVoltage < 13.2f, "Original AP2210 supply operating limit");
static_assert(maxConverterA < tripConverterA, "Current limit/trip order");
}
