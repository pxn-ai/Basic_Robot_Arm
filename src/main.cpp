#include <Arduino.h>
#include <Servo.h>

Servo baseServo;
Servo armServo;
Servo gripServo;

int basePin = 25;
int armPin = 26;
int gripPin = 27;

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