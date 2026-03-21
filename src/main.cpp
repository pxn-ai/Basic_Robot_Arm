/*#include <Arduino.h>
#include <ESP32Servo.h>

Servo baseServo;
Servo armServo;
Servo gripServo;

//  Use safe GPIOs for ESP32-S3
int basePin = 6;
int armPin = 5;
int gripPin = 4;

// Positions
int basePick = 90;
int armPick = 90;
int gripOpen = 90;
int gripClose = 40;

int baseDrop = 120;
int armLift = 70;


//finish positions
int basePic_end = 90;
int armPick_end = 90;




void setup() {
  Serial.begin(115200);

  baseServo.attach(basePin);
  armServo.attach(armPin);
  gripServo.attach(gripPin);

  // Initial position
  baseServo.write(70);
  armServo.write(90);
  gripServo.write(00);

  delay(2000);
  
}

void loop() {

  //int basePick = 00;
  /*for(int pos = 70; pos <= 140; pos++) {
  baseServo.write(pos);
  delay(20);   // increase this (e.g., 30–50) for slower speed
}

// wait after reaching
delay(3000);

// Move back from 135 to 45 slowly
for(int pos = 140; pos >= 70; pos--) {
  baseServo.write(pos);
  delay(20);
}

delay(2000);*/
/*for(int pos = 70; pos <= 140; pos++) {
  armServo.write(pos);
  delay(20);   // increase this (e.g., 30–50) for slower speed
}

// wait after reaching
delay(3000);

// Move back from 135 to 45 slowly
for(int pos = 140; pos >= 70; pos--) {
  armServo.write(pos);
  delay(20);
}

delay(2000);
for(int pos = 00; pos <= 120; pos++) {
  gripServo.write(pos);
  delay(20);   // increase this (e.g., 30–50) for slower speed
}

// wait after reaching
delay(3000);

// Move back from 135 to 45 slowly
for(int pos = 120; pos >= 00; pos--) {
  gripServo.write(pos);
  delay(20);
}

delay(2000);



}*/

/////////////////////////////////////////////////////////



#include <Arduino.h>
#include <ESP32Servo.h>

Servo baseServo;
Servo armServo;
Servo gripServo;

int basePin = 4;
int armPin  = 5;
int gripPin = 6;

int baseAngle = 90;
int armAngle  = 90;
int gripAngle = 90;

void setup() {
  Serial.begin(115200);

  baseServo.attach(basePin);
  armServo.attach(armPin);
  gripServo.attach(gripPin);

  Serial.println("Enter: b angle  | a angle | g angle");
  Serial.println("Example: b 90  OR  a 45  OR  g 120");
}

void loop() {
  if (Serial.available()) {

    char servoID = Serial.read();   // read which servo (b/a/g)
    int angle = Serial.parseInt();  // read angle

    angle = constrain(angle, 0, 180);

    if (servoID == 'b') {
      baseAngle = angle;
      baseServo.write(baseAngle);
      Serial.print("Base: ");
      Serial.println(baseAngle);
    }

    else if (servoID == 'a') {
      armAngle = angle;
      armServo.write(armAngle);
      Serial.print("Arm: ");
      Serial.println(armAngle);
    }

    else if (servoID == 'g') {
      gripAngle = angle;
      gripServo.write(gripAngle);
      Serial.print("Gripper: ");
      Serial.println(gripAngle);
    }
  }
}