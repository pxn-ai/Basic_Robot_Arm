#include <Arduino.h>
#include <ESP32Servo.h>

Servo gripServo;

int gripPin = 4;  // change if needed

void setup() {
  gripServo.attach(gripPin);
}

void loop() {
  // Rotate 0 → 180
  for (int angle = 0; angle <= 180; angle += 2) {
    gripServo.write(angle);
    delay(20);
  }

  // Rotate 180 → 0
  for (int angle = 180; angle >= 0; angle -= 2) {
    gripServo.write(angle);
    delay(20);
  }
}