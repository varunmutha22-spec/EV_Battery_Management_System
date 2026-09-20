#define BLYNK_TEMPLATE_ID "TMPL3EQd35gxy"
#define BLYNK_TEMPLATE_NAME "EV Battery Management System"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#include "BatteryManager.h"
#include "FaultManager.h"
#include "LCDManager.h"
#include "TelemetryManager.h"

char ssid[] = "Wokwi-GUEST";
char pass[] = "";

BatteryManager battery;
FaultManager faultManager;
LCDManager lcdManager;
TelemetryManager telemetry;

unsigned long lastSample = 0;

// TEMPORARY TEST — auto fault injection, no typing needed
unsigned long testStartTime = 0;
bool autoTestDone = false;

enum ConnectionState
{
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  BLYNK_CONNECTED
};

ConnectionState connectionState = WIFI_CONNECTING;

unsigned long lastWiFiAttempt = 0;
unsigned long lastBlynkAttempt = 0;

void updateConnection()
{
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED)
  {
    connectionState = WIFI_CONNECTING;

    if (now - lastWiFiAttempt >= 5000)
    {
      lastWiFiAttempt = now;

      WiFi.begin(ssid, pass, 6);

      Serial.println("WiFi connection attempt...");
    }

    return;
  }

  if (connectionState == WIFI_CONNECTING)
  {
    connectionState = WIFI_CONNECTED;

    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
  }

  if (!Blynk.connected())
  {
    connectionState = WIFI_CONNECTED;

    if (now - lastBlynkAttempt >= 5000)
    {
      lastBlynkAttempt = now;

      Serial.println("Blynk connection attempt...");
      Blynk.connect(100);
    }
  }
  else
  {
    if (connectionState != BLYNK_CONNECTED)
    {
      connectionState = BLYNK_CONNECTED;
      Serial.println("Blynk Cloud connected!");
    }

    // A real communication event -- this is the ONLY place this
    // should be called. (The old bug called it every loop
    // iteration unconditionally, which made COMMUNICATION_FAULT
    // impossible to ever trigger.)
    faultManager.updateCommunication();
  }
}

// --------------------------------------------------------------
// Simple serial command interface for demonstrating fault
// injection and recovery without extra hardware (Task 2 / Task 5
// require you to DEMONSTRATE fault injection and recovery).
//
// Type one of these into the Wokwi Serial Monitor:
//   INJECT_ADC        -> forces an ADC_FAULT
//   INJECT_RELAY      -> forces a RELAY_FAULT
//   INJECT_COMM       -> forces a COMMUNICATION_FAULT
//   CLEAR             -> clears any injected fault
//   STATUS             -> prints current state/fault/risk snapshot
// --------------------------------------------------------------
bool injectedFaultActive = false;

void handleSerialCommands()
{
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd == "INJECT_ADC")
  {
    faultManager.setFault(ADC_FAULT);
    injectedFaultActive = true;
    Serial.println(">> Injected ADC_FAULT");
  }
  else if (cmd == "INJECT_RELAY")
  {
    faultManager.setFault(RELAY_FAULT);
    injectedFaultActive = true;
    Serial.println(">> Injected RELAY_FAULT");
  }
  else if (cmd == "INJECT_COMM")
  {
    faultManager.setFault(COMMUNICATION_FAULT);
    injectedFaultActive = true;
    Serial.println(">> Injected COMMUNICATION_FAULT");
  }
  else if (cmd == "CLEAR")
  {
    faultManager.clearFault();
    injectedFaultActive = false;
    Serial.println(">> Fault cleared");
  }
  else if (cmd == "STATUS")
  {
    Serial.print(">> State: "); Serial.print(faultManager.getStateName());
    Serial.print(" | Fault: "); Serial.print(faultManager.getFaultName());
    Serial.print(" | Risk: "); Serial.print(telemetry.getRiskScore(battery, faultManager), 1);
    Serial.print(" | Uptime(s): "); Serial.println(faultManager.getUptimeSeconds());
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  battery.begin();
  faultManager.begin();
  lcdManager.begin();
  telemetry.begin();

  WiFi.begin(ssid, pass, 6);
  Blynk.config(BLYNK_AUTH_TOKEN);
}

void loop() {
  Blynk.run();
  updateConnection();
  handleSerialCommands();

  if (millis() - lastSample >= SAMPLE_INTERVAL) {

    lastSample = millis();

    battery.update();

    Serial.print("GPIO34 = ");
    Serial.println(analogRead(34));

    lcdManager.update(battery, faultManager);

        // TEMPORARY TEST — no typing into Serial needed
    if (testStartTime == 0) testStartTime = millis();
    unsigned long testElapsed = millis() - testStartTime;

   // if (testElapsed > 3000 && testElapsed < 15000)
    //{
      //faultManager.setFault(ADC_FAULT);   // fake the fault automatically
      //injectedFaultActive = true;
     //Serial.println(">> AUTO-INJECTED ADC_FAULT (test)");
    //}
    //else if (testElapsed >= 15000 && injectedFaultActive)
    //{
      //faultManager.clearFault();
      //injectedFaultActive = false;
      //Serial.println(">> AUTO-CLEARED fault (test)");
    //}

    // Injected faults (via serial) stay active until CLEAR is sent;
    // otherwise let the real sensor/battery checks drive the fault.
    if (!injectedFaultActive)
    {
      faultManager.checkBatteryFault(
          battery.getImbalance(),
          battery.getAdaptiveThreshold()
      );

      faultManager.checkADCFault(
          battery.hasADCFault()
      );
    }

    Serial.print("Fault ID: ");
    Serial.println(faultManager.getFault());

    faultManager.update();

    switch (faultManager.getState())
    {
      case NORMAL:

        faultManager.requestRelayState(true);

        digitalWrite(GREEN_LED_PIN, HIGH);
        digitalWrite(YELLOW_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, LOW);

        digitalWrite(BUZZER_PIN, HIGH);

        break;

      case DEGRADED:

        faultManager.requestRelayState(true);

        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(YELLOW_LED_PIN, HIGH);
        digitalWrite(RED_LED_PIN, LOW);

        digitalWrite(BUZZER_PIN, HIGH);

        break;

      case FAILSAFE:

        faultManager.requestRelayState(false);

        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(YELLOW_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, HIGH);

        digitalWrite(BUZZER_PIN, LOW);

        break;

      case SHUTDOWN:

        faultManager.requestRelayState(false);

        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(YELLOW_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, HIGH);

        digitalWrite(BUZZER_PIN, LOW);

        break;
    }

    faultManager.updateRelay();

    // --- Relay mismatch check (Task 4) ---
    // Tell FaultManager what we just commanded, then compare it
    // against the pin's actual state. On real hardware this would
    // read a separate feedback line; in this simulation digitalRead()
    // on the same OUTPUT pin reflects what was actually latched,
    // which is what INJECT_RELAY (above) is for -- demonstrating a
    // forced mismatch scenario for your video.
    faultManager.setCommandedRelayState(faultManager.getRelayState());
    bool relayFeedback = digitalRead(RELAY_PIN);
    faultManager.checkRelayMismatch(relayFeedback);

    // --- Task 5: event-driven telemetry + offline queue + RSSI ---
    telemetry.update(battery, faultManager, Blynk.connected());

    if (telemetry.hasPendingSend())
    {
      TelemetryEvent e = telemetry.getPendingEvent();
      bool fromQueue = telemetry.isPendingFromQueue();

                Blynk.virtualWrite(V0, e.packVoltage);
                Blynk.virtualWrite(V1, e.avgSOC);
                Blynk.virtualWrite(V2, e.imbalance);

                Blynk.virtualWrite(V10, battery.getCellVoltage(0));
                Blynk.virtualWrite(V11, battery.getCellVoltage(1));
                Blynk.virtualWrite(V12, battery.getCellVoltage(2));
                Blynk.virtualWrite(V13, battery.getCellVoltage(3));

                Blynk.virtualWrite(V3, "C" + String(e.weakestIdx + 1) + ": " + String(e.weakestV, 2) + "V");
                Blynk.virtualWrite(V4, "C" + String(e.strongestIdx + 1) + ": " + String(e.strongestV, 2) + "V");
                Blynk.virtualWrite(V5, e.relayState ? 1 : 0);

                String stateStr;
      switch (e.state)
      {
        case NORMAL:    stateStr = "NORMAL";    break;
        case DEGRADED:  stateStr = "DEGRADED";  break;
        case FAILSAFE:  stateStr = "FAILSAFE";  break;
        case SHUTDOWN:  stateStr = "SHUTDOWN";  break;
      }
      Blynk.virtualWrite(V6, stateStr);
      Blynk.virtualWrite(V7, telemetry.getRSSI());
      Blynk.virtualWrite(V8, telemetry.getQueueDepth());

      // Task 6: risk score sent alongside the same event
      Blynk.virtualWrite(V9, telemetry.getRiskScore(battery, faultManager));

      if (telemetry.getQueueDepth() > 0)
    {
      Blynk.virtualWrite(V14, "QUEUED");
    }
      else
    {
      Blynk.virtualWrite(V14, "LIVE");
    }
     String executiveSummary =
    "Health: " + telemetry.getBatteryHealth(battery, faultManager) +
    " | Uptime: " + String(faultManager.getUptimeSeconds()) +
    "s | Faults: " + String(faultManager.getFaultCount()) +
    " | State: " + faultManager.getStateName();

Blynk.virtualWrite(V15, executiveSummary);
Blynk.virtualWrite(V16, telemetry.getBatteryHealth(battery, faultManager));
Blynk.virtualWrite(V17, telemetry.getImbalanceTrendValue());
Blynk.virtualWrite(V18, telemetry.getRiskTrendValue());
Blynk.virtualWrite(V19, faultManager.getFaultCount());
Blynk.virtualWrite(V20, faultManager.getUptimeSeconds());
Blynk.virtualWrite(V21, telemetry.getMaintenanceSuggestion(battery, faultManager));
Blynk.virtualWrite(V22, faultManager.getStateName());

// Task 6: Severity indicators
SystemState currentState = faultManager.getState();

Blynk.virtualWrite(V23, currentState == NORMAL ? 1 : 0);
Blynk.virtualWrite(V24, currentState == DEGRADED ? 1 : 0);
Blynk.virtualWrite(V25, currentState == FAILSAFE ? 1 : 0);
Blynk.virtualWrite(V26, currentState == SHUTDOWN ? 1 : 0);

      Serial.print(fromQueue ? "[QUEUE->CLOUD] " : "[LIVE] ");
      Serial.print("Sent telemetry @ t=");
      Serial.println(e.timestamp);

      telemetry.markSent();
    }
    Serial.println("========== Battery Status ==========");

    for (int i = 0; i < NUM_CELLS; i++) {

      Serial.print("Cell ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(battery.getCellVoltage(i), 2);
      Serial.print(" V");

      Serial.print(" | SoC: ");
      Serial.print(battery.getCellSOC(i), 1);
      Serial.println("%");
    }

    Serial.print("Pack Voltage: ");
    Serial.print(battery.getPackVoltage(), 2);
    Serial.println(" V");

    Serial.print("Average SoC: ");
    Serial.print(battery.getAverageSOC(), 1);
    Serial.println("%");

    Serial.print("Adaptive Threshold: ");
    Serial.print(battery.getAdaptiveThreshold(), 2);
    Serial.println(" V");

    Serial.print("Highest Cell: ");
    Serial.println(battery.getHighestCellIndex() + 1);

    Serial.print("Lowest Cell: ");
    Serial.println(battery.getLowestCellIndex() + 1);

    Serial.print("Imbalance: ");
    Serial.print(battery.getCurrentImbalance(), 2);
    Serial.println(" V");

    Serial.print("Trend: ");
    Serial.println(battery.getImbalanceTrend());
    Serial.print("System State: ");
    Serial.println(faultManager.getStateName());

    Serial.print("Risk Score: ");
    Serial.println(telemetry.getRiskScore(battery, faultManager), 1);

    Serial.print("Suggestion: ");
    Serial.println(telemetry.getMaintenanceSuggestion(battery, faultManager));

    Serial.print("Offline Queue Depth: ");
    Serial.println(telemetry.getQueueDepth());

    Serial.print("WiFi RSSI: ");
    Serial.println(telemetry.getRSSI());

    Serial.println();
  }
}
