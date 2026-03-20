#include <Arduino.h>
#include <ESP32Servo.h>

Servo baseServo;
Servo armServo;
Servo gripServo;

// ⚠️ Use safe GPIOs for ESP32-S3
int basePin = 5;
int armPin = 6;
int gripPin = 7;

// Positions
int basePick = 60;
int armPick = 120;
int gripOpen = 90;
int gripClose = 40;

int baseDrop = 120;
int armLift = 70;

void setup() {
  Serial.begin(115200);

  baseServo.attach(basePin);
  armServo.attach(armPin);
  gripServo.attach(gripPin);

  // Initial position
  baseServo.write(90);
  armServo.write(90);
  gripServo.write(gripOpen);

  delay(2000);
}

void loop() {

  // Move to pick position
  baseServo.write(basePick);
  armServo.write(armPick);
  delay(1000);

  // Close gripper
  gripServo.write(gripClose);
  delay(1000);

  // Lift
  armServo.write(armLift);
  delay(1000);

  // Move to drop
  baseServo.write(baseDrop);
  delay(1000);

  // Lower
  armServo.write(armPick);
  delay(1000);

  // Release
  gripServo.write(gripOpen);
  delay(1000);

  // Lift back
  armServo.write(armLift);
  delay(1000);

  // Return center
  baseServo.write(90);
  delay(1000);
}
























#include <Arduino.h>
#include <Servo.h>

Servo baseServo;
Servo armServo;
Servo gripServo;

int basePin = 6;
int armPin = 5;
int gripPin = 4;

void setup() {

  baseServo.attach(basePin);
  armServo.attach(armPin);
  gripServo.attach(gripPin);

  baseServo.write(90);
  armServo.write(90);
  gripServo.write(90);

  delay(1000);
}

void loop() {

  for(int pos=0; pos<=180; pos++)
  {
    baseServo.write(pos);
    armServo.write(180-pos);
    gripServo.write(pos/2);

    delay(15);
  }

  for(int pos=180; pos>=0; pos--)
  {
    baseServo.write(pos);
    armServo.write(180-pos);
    gripServo.write(pos/2);

    delay(15);
  }

}