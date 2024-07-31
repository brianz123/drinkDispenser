/*
 * PCF8575    ----- WeMos
 * A0         ----- GRD
 * A1         ----- GRD
 * A2         ----- GRD
 * VSS        ----- GRD
 * VDD        ----- 5V/3.3V
 * SDA        ----- GPIO_4(PullUp)
 * SCL        ----- GPIO_5(PullUp)
 *
 * P0     ----------------- LED0
 * P1     ----------------- LED1
 * P2     ----------------- LED2
 * P3     ----------------- LED3
 * P4     ----------------- LED4
 * P5     ----------------- LED5
 * P6     ----------------- LED6
 * P7     ----------------- LED7
 * P8     ----------------- LED8
 * P9     ----------------- LED9
 * P10     ----------------- LED10
 * P11     ----------------- LED11
 * P12     ----------------- LED12
 * P13     ----------------- LED13
 * P14     ----------------- LED14
 * P15     ----------------- LED15
*/
#define DEBUG false
#ifdef DEBUG
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif


#include "PCF8575.h"   // https://github.com/xreef/PCF8575_library
#include <ezButton.h>  // the library to use for SW pin
#include "VarSpeedServo.h"
#include <Encoder.h>
#include <L298N.h>
//io exp on A4 and A5
#define CLK_PIN 2
// #define INT 9
#define DT_PIN 4
#define SW_PIN 6
#define DIRECTION_CW 0   // clockwise direction
#define DIRECTION_CCW 1  // counter-clockwise direction
#define RESET_PIN 9
#define NUM_CUPS 6
#define IN1 7
#define IN2 8
#define EN 5
#define POUR_DURATION 500  //Need to test (in milliseconds)
#define SERVO_PIN 11
#define BUZZER 10
#define home 180
#define dir 1

volatile int counter = 0;
volatile int direction = DIRECTION_CW;
volatile unsigned long last_time;  // for debouncing
int prev_counter;
int rst = false;
bool go = true;
bool lightsOn = true;
int angleVar = 16;
const int whiteLeds[] = { 0, 1, 2, 3, 4, 5 };
const int redLeds[] = { 15, 14, 13, 12, 11, 10 };
ezButton button(SW_PIN);  // create ezButton object that attach to pin 4
ezButton reset(RESET_PIN);
L298N motor(EN, IN1, IN2);
VarSpeedServo myservo;  // create servo object to control a servo
int oldPosition = -999;
long newPosition = 0;
// Set i2c address
PCF8575 pcf8575(0x20);
Encoder myEnc(DT_PIN, CLK_PIN);
void setup() {
  myservo.attach(11);             // attaches the servo on pin 9 to the servo object
  myservo.write(home, 11, true);  // set the intial position of the servo, as fast as possible, delay until done //////Need to deside initial position
  Serial.begin(115200);
  debugln("Starting Up!");
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, INPUT);
  pinMode(BUZZER, OUTPUT);
  button.setDebounceTime(100);  // set debounce time to 50 milliseconds
  button.setCountMode(COUNT_RISING);
  reset.setDebounceTime(100);
  motor.setSpeed(255);


  // use interrupt for CLK pin is enough
  // call ISR_encoderChange() when CLK pin changes from LOW to HIGH
  // attachInterrupt(digitalPinToInterrupt(CLK_PIN), ISR_encoderChange, RISING);
  // attachInterrupt(digitalPinToInterrupt(RESET_PIN), ISR_rst, RISING);


  // Set pinMode to OUTPUT
  for (int i = 0; i < 16; i++) {
    pcf8575.pinMode(i, OUTPUT);
  }
  pcf8575.begin();
  debugln("Ready!");
  // for (int j = 0; j < 1; j++) {
  for (int k = 230; k < 255; k += 5) {
    chirp(100, 200);
    delay(50);
    chirp(100, 150);
    delay(50);
  }
  // }
}

void loop() {

  button.loop();
  reset.loop();
  motor.stop();
  newPosition = myEnc.read();
  encoderChange();
  if (digitalRead(SW_PIN) == LOW) pour();
  if (lightsOn) {
    for (int j = 0; j < NUM_CUPS; j++) {
      setLED(j, LOW, 2);
    }
    for (int i = counter; i < NUM_CUPS; i++) {
      setLED(i, LOW, 1);
    }
    for (int c = 0; c < counter; c++) {
      setLED(c, HIGH, 1);
    }
  }
  //check if it needs to prime
  if (!digitalRead(RESET_PIN)) {
    debugln("Prime??");
    for (int i = 0; i < NUM_CUPS; i++) {
      setLED(i, true, 2);
    }
    delay(1500);
    if (!digitalRead(RESET_PIN)) prime();
    for (int i = 0; i < NUM_CUPS; i++) {
      setLED(i, false, 2);
    }
  }
}


void encoderChange() {

  if (newPosition != oldPosition) {
    oldPosition = newPosition;
    counter = (newPosition / 4);
    if ((millis() - last_time) < 100)  // debounce time is 50ms
      return;

    last_time = millis();

    if (counter > NUM_CUPS) {
      counter = NUM_CUPS;
      myEnc.write(NUM_CUPS * 4);  //needs to be mult by 4, not sure why
      debugln("MAX");
    } else if (counter < 0) {
      counter = 0;
      myEnc.readAndReset();
    }
    debugln(counter);
    lightsOn = true;
  }
  return;
}
void pour() {
  //   int list[] = { 135, 115, 98, 79, 60, 42 };
  int list[] = { 150, 133, 113, 95, 75, 55 };
  myservo.write(58, 100, true);
  debug("Pouring ");
  debug(counter);
  debugln(" cups.");
  int angle = 5;
  rst = false;
  if (counter > NUM_CUPS) counter = NUM_CUPS;
  for (int i = counter; i >= 1; i--) {
    debug("Pouring cup number ");
    debugln(counter - i + 1);
    myservo.write(list[i - 1], 45, true);
    // if (i == counter) {
    //   dispense(true);
    //   delay(75);
    // }
    dispense(true);
    delay(POUR_DURATION);
    debug("elapsed time: ");
    debugln(POUR_DURATION);
    motor.stop();
    setLED(i - 1, true, 2);
    setLED(i - 1, false, 1);
    for (int j = 0; j > 3; j++) {
      chirp(100, 200);
      delay(250);
      chirp(100, 180);
      delay(250);
    }
    delay(1000);
  }
  myservo.write(home, 15, true);
  lightsOn = false;
  return;
}

void prime() {
  debugln("Priming");
  counter = 0;
  int prevCounter = counter;

  //flash all LEDs 6 times
  for (int j = 0; j < 6; j++) {
    for (int i = 0; i < NUM_CUPS; i++) {
      setLED(i, false, 1);
      setLED(i, false, 2);
    }
    delay(100);
    for (int i = 0; i < NUM_CUPS; i++) {
      setLED(i, true, 1);
      setLED(i, true, 2);
    }
    delay(100);
  }
  int count = 0;
  go = true;
  button.resetCount();
  myservo.write(100, 35, true);
  myservo.write(150, 25, true);
  for (int i = 0; i < NUM_CUPS; i += 2) {
    setLED(i, true, 1);
    setLED(i + 1, true, 2);
  }
  //turn on pump for short intervals of time to prime tube
  while (go) {
    button.loop();
    count = button.getCount();
    if (count > prevCounter) {
      prevCounter = count;
      dispense(true);
      delay(500);
      motor.stop();
      debug("Pour: ");
      debugln(count);
      debugln(counter);
    }
    if (!digitalRead(RESET_PIN) == true) go = false;
  }
  dispense(false);
  delay(1000);
  motor.stop();
  //turn LEDs off to show that priming is complete
  for (int i = 0; i < NUM_CUPS; i++) {
    setLED(i, false, 1);
    setLED(i, false, 2);
  }
  debugln("Priming complete");
  myservo.write(home, 15, true);
  return;
}


void setLED(int w, bool state, int color) {
  if (color == 1)
    pcf8575.digitalWrite(redLeds[w], state);
  if (color == 2)
    pcf8575.digitalWrite(whiteLeds[w], state);
  return;
}




void dispense(bool out) {
  if (out) {  //pour
    if (dir == 1)
      motor.forward();
    else
      motor.backward();
  } else {  //suck
    if (dir == 1) {
      motor.backward();
    } else {
      motor.forward();
    }
  }
}
void chirp(int duration, int freq) {
  for (int i = 0; i < duration * 2; i++) {
    digitalWrite(BUZZER, 255);
    delayMicroseconds(freq);
    digitalWrite(BUZZER, 0);
    delayMicroseconds(freq);
  }
  return;
}