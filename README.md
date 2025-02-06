# W211-Gear-Selector-Emulator

## Background

This project was born out of necessity when I converted my **2005 Mercedes w211 E320** from an automatic gearbox to a manual one. After removing the original CAN bus gear selector, I encountered two major issues:
- **No Start:** Without the original gear selector communicating with the car's systems, the vehicle wouldn’t start.
- **Missing Functions:** With the car was running, the reverse lights and parking sensors could not be activated.

## Overview

- **CAN Bus Communication:**  
  The program uses the [MCP_CAN library](https://github.com/coryjfowler/MCP_CAN_lib) to initialize and communicate over the CAN bus at 500 kbps.

- **Gear Priority Logic:**  
  The code selects the gear based on the following rules:
  - **Reverse:** When the reverse switch (connected to pin 8) is pressed.
  - **Drive:** When the handbrake is disengaged (handbrake signal received via CAN indicates it’s off).
  - **Park:** When the handbrake is engaged (handbrake signal received via CAN indicates it’s on).

- **LED Indicator:**  
  The onboard LED (pin 13) reflects the gear state:
  - **Park:** LED stays solid **ON**.
  - **Drive:** LED remains **OFF**.
  - **Reverse:** LED pulses (toggled every 500 ms).

- **Serial Debugging:**  
  The program prints messages to the Serial Monitor (at 115200 baud) whenever the gear or handbrake state changes.

## Hardware Setup

- **CANBus Board:**  
  - Connected via SPI.
  - **Chip Select (CS) Pin:** GPIO 17.

- **Reverse Switch:**  
  - Connected to digital pin 8.
  - Configured with an internal pull-up resistor.

- **Onboard LED:**  
  - Connected to digital pin 13 (default LED pin on many Arduino boards).

## Software Details

### Libraries

- **SPI.h:** Standard library for SPI communication.
- **mcp_canbus.h:** Library to interface with the MCP CAN bus controller.

### Global Variables and Timing

- **CAN Message Intervals:**
  - Gear messages are sent every 10 ms.
  - Handbrake status is checked every 20 ms.

- **State Variables:**
  - `handbrakeOn` & `lastHandbrakeState`: Track the current and previous handbrake status.
  - `lastGearSent`: Remembers the last gear sent over CAN to avoid redundant messages.
  - LED pulsing variables manage the toggling for the Reverse gear.

### Code Walkthrough

#### 1. Setup

The `setup()` function initializes the Serial communication, configures the input/output pins, and sets up the CAN bus:


```cpp
void setup() {
    Serial.begin(115200);
    if (!Serial) {
        delay(1000); // Allow time for the Serial port to initialize
    }

    pinMode(REVERSE_SWITCH_PIN, INPUT_PULLUP); // Reverse switch as input with pull-up
    pinMode(LED_PIN, OUTPUT); // LED as output

    // Initialize CAN at 500kbps; retry until successful
    while (CAN_OK != CAN.begin(CAN_500KBPS)) {
        Serial.println("CAN BUS FAIL!");
        delay(100);
    }
    Serial.println("CAN BUS OK!");

    // Configure CAN masks and filters:
    // Masks: 0x7FF ensures that all bits are considered
    CAN.init_Mask(0, 0, 0x7FF);
    CAN.init_Mask(1, 0, 0x7FF);

    // Filters: Accept only messages with CAN ID 0x240
    CAN.init_Filt(0, 0, 0x240);
    // Additional filters are commented out but can be enabled if needed:
    // CAN.init_Filt(1, 0, 0x240);
    // ...
}
```
#### 2. Main Loop
The loop() function is responsible for:

* Periodically checking for new CAN messages to update the handbrake status.
* Determining the current gear based on the reverse switch and handbrake status.
* Sending the corresponding gear command over the CAN bus.
* Updating the LED indicator accordingly.

```ccp
 void loop() {
    unsigned long currentTime = millis();

    // Check handbrake status every 20 ms
    if (currentTime - lastHandbrakeCheckTime >= interval20ms) {
        handleHandbrake();
        lastHandbrakeCheckTime = currentTime;
    }

    // Gear priority logic:
    // 1. Reverse switch pressed: Reverse gear (0x07)
    // 2. Handbrake off: Drive gear (0x09)
    // 3. Otherwise: Park (0x08)
    unsigned char currentGear;
    if (digitalRead(REVERSE_SWITCH_PIN) == LOW) { // Reverse pressed
        currentGear = 0x07; // Reverse
        sendGear(currentGear, "Reverse");
    } else if (!handbrakeOn) { // Handbrake disengaged
        currentGear = 0x09; // Drive
        sendGear(currentGear, "Drive");
    } else { // Default to Park
        currentGear = 0x08; // Park
        sendGear(currentGear, "Park");
    }

    // Update the LED based on the current gear
    updateLED(currentGear);
}`
```

#### 3. Sending Gear Commands
The sendGear() function sends a CAN message with the gear information every 10 ms and prints to Serial only if the gear changes:

```ccp
 void sendGear(unsigned char gear, const char* gearName) {
    unsigned long currentTime = millis();

    if (currentTime - lastGearTime >= interval10ms) {
        unsigned char data[1] = {gear};
        CAN.sendMsgBuf(0x230, 0, 1, data);

        // Print gear change only if the gear is different from the last sent value
        if (lastGearSent != gear) {
            Serial.print("Sent: ");
            Serial.println(gearName);
            lastGearSent = gear;
        }

        lastGearTime = currentTime;
    }
}`
```

#### 4. Handling Handbrake Status
The handleHandbrake() function checks for incoming CAN messages (expected to be with ID 0x240) and updates the handbrake state:

```ccp
 void handleHandbrake() {
    unsigned char len = 0;
    unsigned char buf[8];

    if (CAN_MSGAVAIL == CAN.checkReceive()) {
        CAN.readMsgBuf(&len, buf);
        unsigned long canId = CAN.getCanId();

        // With our filter, we only process messages with ID 0x240
        if (canId == 0x240 && len == 8) {
            // Determine handbrake state based on byte 4 of the message
            bool currentHandbrakeState = (buf[4] == 0x10 || buf[4] == 0x90);

            if (currentHandbrakeState != lastHandbrakeState) {
                Serial.println(currentHandbrakeState ? "Handbrake is ON" : "Handbrake is OFF");
                lastHandbrakeState = currentHandbrakeState;
            }

            handbrakeOn = currentHandbrakeState;
        }
    }
}`
```

#### 5. LED Indicator Update
The updateLED() function sets the LED state based on the gear:

* Park (0x08): LED is solid ON.
* Drive (0x09): LED is turned OFF.
* Reverse (0x07): LED pulses by toggling every 500 ms.
