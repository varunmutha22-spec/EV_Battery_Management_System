#define BLYNK_TEMPLATE_ID "TMPL3EQd35gxy"
#define BLYNK_TEMPLATE_NAME "EV Battery Management System"
#define BLYNK_AUTH_TOKEN "fNPQ7Y4LK5ZRvg8TFBnXqPRiLPtVIvEe"

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

//==================================================
// Battery Configuration
//==================================================

// Change only this value to scale the BMS
#define NUM_CELLS 4

// ADC pins for each battery cell
const uint8_t CELL_PINS[NUM_CELLS] =
{
  34,
  35,
  32,
  33
};

//==================================================
// ADC Configuration
//==================================================

const float ADC_REFERENCE = 3.3;
const int ADC_RESOLUTION = 4095;

//==================================================
// Cell Voltage Limits
//==================================================

const float CELL_MIN_VOLTAGE = 3.00;
const float CELL_MAX_VOLTAGE = 4.20;

//==================================================
// Timing
//==================================================

const unsigned long SAMPLE_INTERVAL = 500;

#define SIMULATE_FROZEN_ADC false

// Output Pins
#define RELAY_PIN     27
#define RED_LED_PIN   25
#define YELLOW_LED_PIN 26
#define GREEN_LED_PIN 14
#define BUZZER_PIN    13

#define IMBALANCE_HIGH_THRESHOLD 0.30
#define IMBALANCE_LOW_THRESHOLD  0.25

//==================================================
// Telemetry / Offline Queue
//==================================================

#define TELEMETRY_QUEUE_SIZE 20
#define TELEMETRY_HEARTBEAT_MS 30000UL
#define TELEMETRY_MIN_PACK_DELTA 0.05
#define TELEMETRY_MIN_IMBALANCE_DELTA 0.02

#endif
