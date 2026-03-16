#include <Arduino.h>
#include <Servo.h>

Servo servo1;
Servo servo2;
Servo servo3;

void setup() {
    servo1.attach(25); // GPIO for servo1
    servo2.attach(26); // GPIO for servo2
    servo3.attach(27); // GPIO for servo3
}

void loop() {
    // Sweep example
    for (int pos = 0; pos <= 180; pos += 1) {
        servo1.write(pos);
        servo2.write(pos/2);
        servo3.write(180 - pos);
        delay(15);
    }
    for (int pos = 180; pos >= 0; pos -= 1) {
        servo1.write(pos);
        servo2.write(pos/2);
        servo3.write(180 - pos);
        delay(15);
    }
}