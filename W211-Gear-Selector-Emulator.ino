#include <SPI.h>
#include <mcp_canbus.h>

#define SPI_CS_PIN  17  // CS Pin for CANBed board
#define REVERSE_SWITCH_PIN 8 // GPIO pin for reverse switch
#define LED_PIN 13 // Onboard LED

MCP_CAN CAN(SPI_CS_PIN);  // Create MCP_CAN instance

// Function prototypes
void sendGear(unsigned char gear, const char* gearName);
void handleHandbrake();
void updateLED(unsigned char gear);

// CAN message intervals
unsigned long lastGearTime = 0;
unsigned long lastHandbrakeCheckTime = 0;
const unsigned long interval10ms = 10;
const unsigned long interval20ms = 10;

// Handbrake state
bool handbrakeOn = true;
bool lastHandbrakeState = true; // To track state changes

// Gear tracking
unsigned char lastGearSent = 0x00; // Initialize to an invalid gear state

// LED pulsing variables
unsigned long lastLEDToggleTime = 0;
const unsigned long pulseInterval = 500; // LED pulse interval in milliseconds
bool ledState = LOW;

void setup() {
    Serial.begin(115200);
    if (!Serial) {
    delay(1000); // Optional: Give some time for Serial to initialize if needed
    }

    pinMode(REVERSE_SWITCH_PIN, INPUT_PULLUP); // Configure reverse switch as input with pull-up resistor
    pinMode(LED_PIN, OUTPUT); // Configure LED pin as output

    // Initialize CAN at 500kbps
    while (CAN_OK != CAN.begin(CAN_500KBPS)) {
        Serial.println("CAN BUS FAIL!");
        delay(100);
    }
    Serial.println("CAN BUS OK!");

    // -----------------------------------------------------
    // 1) Configure masks (we use 0x7FF so that we care about all bits)
    // -----------------------------------------------------
    // "0" for standard frames, "0x7FF" is the "match all bits" mask
    CAN.init_Mask(0, 0, 0x7FF);
    CAN.init_Mask(1, 0, 0x7FF);

    // -----------------------------------------------------
    // 2) Configure filters to accept only required ID's 0x240
    // -----------------------------------------------------
    // "0" for standard frame, "0x240" is your exact ID
    CAN.init_Filt(0, 0, 0x240);
    //CAN.init_Filt(1, 0, 0x240);
    //CAN.init_Filt(2, 0, 0x240);
=
}

void loop() {
    unsigned long currentTime = millis();

    // Check handbrake status every 20ms
    if (currentTime - lastHandbrakeCheckTime >= interval20ms) {
        handleHandbrake();
        lastHandbrakeCheckTime = currentTime; // Update the last time handbrake was checked
    }

    // Gear priority logic
    unsigned char currentGear;
    if (digitalRead(REVERSE_SWITCH_PIN) == LOW) { // Reverse switch is pressed
        currentGear = 0x07; // Reverse
        sendGear(currentGear, "Reverse");
    } else if (!handbrakeOn) { // Handbrake is off
        currentGear = 0x09; // Drive
        sendGear(currentGear, "Drive");
    } else { // Default to Park
        currentGear = 0x08; // Park
        sendGear(currentGear, "Park");
    }

    // Update the LED behavior based on the current gear
    updateLED(currentGear);
}

void sendGear(unsigned char gear, const char* gearName) {
    unsigned long currentTime = millis();

    // Send the CAN message every 10ms
    if (currentTime - lastGearTime >= interval10ms) {
        unsigned char data[1] = {gear};
        CAN.sendMsgBuf(0x230, 0, 1, data);

        // Print to Serial only if the gear has changed
        if (lastGearSent != gear) {
            Serial.print("Sent: ");
            Serial.println(gearName);
            lastGearSent = gear;
        }

        lastGearTime = currentTime; // Update the last time a message was sent
    }
}

void handleHandbrake() {
    unsigned char len = 0;
    unsigned char buf[8];

    if (CAN_MSGAVAIL == CAN.checkReceive()) { // Check for incoming CAN messages
        CAN.readMsgBuf(&len, buf); // Read the data
        unsigned long canId = CAN.getCanId();

        // Because of our filter, we *should* only ever see 0x240 here:
        if (canId == 0x240 && len == 8) {
            bool currentHandbrakeState = (buf[4] == 0x10 || buf[4] == 0x90); // Determine current handbrake state

            if (currentHandbrakeState != lastHandbrakeState) { // Only print if state changes
                if (currentHandbrakeState) {
                    Serial.println("Handbrake is ON");
                } else {
                    Serial.println("Handbrake is OFF");
                }
                lastHandbrakeState = currentHandbrakeState;
            }

            handbrakeOn = currentHandbrakeState;
        }
    }
}

void updateLED(unsigned char gear) {
    unsigned long currentTime = millis();

    if (gear == 0x08) { // Park
        digitalWrite(LED_PIN, HIGH); // Turn LED on
    } else if (gear == 0x09) { // Drive
        digitalWrite(LED_PIN, LOW); // Turn LED off
    } else if (gear == 0x07) { // Reverse
        // Pulse the LED
        if (currentTime - lastLEDToggleTime >= pulseInterval) {
            ledState = !ledState; // Toggle the LED state
            digitalWrite(LED_PIN, ledState);
            lastLEDToggleTime = currentTime; // Update the last toggle time
        }
    }
}

