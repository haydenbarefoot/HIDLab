
/**
 * HIDLab firmware entry point.
 * Target: Arduino Nano ESP32
 *
 * Initial hardware validation sketch.
 */

void setup() {
    // The built-in LED provides a hardware test without peripherals.
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);

    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
}
