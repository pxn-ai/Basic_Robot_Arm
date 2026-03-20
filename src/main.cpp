#include <Arduino.h>
#include <ESP32Servo.h>

Servo baseServo;
Servo armServo;
Servo gripServo;

//  Use safe GPIOs for ESP32-S3
int basePin = 5;
//int armPin = 5;
//int gripPin = 7;

// Positions
int basePick = 90;
int armPick = 90;
int gripOpen = 90;
int gripClose = 40;

int baseDrop = 120;
int armLift = 70;

void setup() {
  Serial.begin(115200);

  baseServo.attach(basePin);
  //armServo.attach(armPin);
  //gripServo.attach(gripPin);

  // Initial position
  baseServo.write(00);
  //armServo.write(00);
  //gripServo.write(gripOpen);

  delay(2000);
}

void loop() {

  //int basePick = 00;
  baseServo.write(00);
  delay(2000);
  baseServo.write(180);
  delay(5000);

}