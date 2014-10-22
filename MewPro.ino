// MewPro
//
// The following are known to work with MewPro at least core functionalities. Not all the sensors, however, are supported by each of them.
//
//   Arduino Pro Mini 328 3.3V 8MHz (also overclocked version of 3.3V 16MHz)
//   Arduino Pro 328 3.3V 8MHz
//   Arduino Due
//          w/ Arduino IDE 1.5.7+
//          if you have troubles on compiling unused or nonexistent libraries, simply comment out #include line as //#include (see Note below)
//
//   Teensy 3.x
//          To compile the code with Teensy 3.x:
//          1. use Arduino IDE 1.0.6+ and Teensyduino 1.20+
//          2. comment out all unused #include as //#include (see Note below)
//
//   (Note: There is an infamous Arduino IDE's preprocessor bug (or something) that causes to ignore #ifdef/#else/#endif directives and forces 
//    to compile unnecessary libraries.)
//
//   GR-KURUMI
//          To compile the code with GR-KURUMI using Renesas web compiler http://www.renesas.com/products/promotion/gr/index.jsp#cloud :
//          1. open a new project with the template GR-KURUMI_Sketch_V1.04.zip.
//          2. create a folder named MewPro and upload all the files there.
//          3. at project's home directory, replace all the lines of gr_scketch.cpp by the following code (BEGIN / END lines should be excluded).
/* BEGIN copy
#include <RLduino78.h>

#include "MewPro/MewPro.ino"
#include "MewPro/a_Queue.ino"
#include "MewPro/b_TimeAlarms.ino"
#include "MewPro/c_I2C.ino"
#include "MewPro/d_BacpacCommands.ino"
#include "MewPro/e_Shutter.ino"
#include "MewPro/f_Switch.ino"
#include "MewPro/g_IRremote.ino"
#include "MewPro/h_LightSensor.ino"
#include "MewPro/i_PIRsensor.ino"
#include "MewPro/j_VideoMotionDetect.ino"
END copy */
//
//   Copyright (c) 2014 orangkucing

#include <Arduino.h>

// deal w/ interrupt pin numbers
#if defined (__arm__) && defined (__SAM3X8E__) || defined(__MK20DX256__) || defined(__MK20DX128__) // Arduino Due & Teensy 3.x
#define INT_NUMBER_OFFSET 0
#else
#define INT_NUMBER_OFFSET (-2)
#endif

// Options:
//   Choose either "#define" to use or "#undef" not to use. 
#undef  USE_TIME_ALARMS      // a_TimeAlarms: MewPro driven timelapse
#undef  USE_SHUTTERS         // e_Shutters: One or two remote shutters without contact bounce or chatter
#undef  USE_SWITCHES         // f_Switches: One or two mechanical switches
#undef  USE_IR_REMOTE        // g_IRremote: IR remote controller
#undef  USE_LIGHT_SENSOR     // h_LightSensor: Ambient light sensor
#undef  USE_PIR_SENSOR       // i_PIRsensor: Passive InfraRed motion sensor
//   Video motion detect consumes almost all the dynamic memory. So if you want to use this then #undef all options above.
#undef  USE_VIDEOMOTION      // j_VideoMotionDetect: Video Motion Detector

// Arduino pins
// Assignment of these pins (except 10-13/A0-A1 or I2C's SCL/SDA) can be re-configured here.
//
//                           0;  // (Used by serial port)
//                           1;  // (Used by serial port)
const int SHUTTER_PIN      = 2;  // Interrupt pin w/o software debounce
//                           3;  // (not in use)
const int IRRECV_PIN       = 4;  // IR remote controller
const int SWITCH0_PIN      = 5;  // Software debounced; ON-start ON-stop
const int SWITCH1_PIN      = 6;  // Software debounced; ON-start OFF-stop
//                           7;  // (Arduino: Not in use; GR-KURUMI: Used by I2C SCL)
//                           8;  // (Arduino: Not in use; GR-KURUMI: Used by I2C SDA)
const int PIR_PIN          = 9;  // Passive InfraRed motion sensor
// BEGIN Don't change the following pin allocations. These are used to control Herobus. 
const int I2CINT           = 10;
const int TRIG             = 11;
const int BPRDY            = 12; // Pulled up by camera
#ifndef REL_GR_KURUMI
const int LED_OUT          = 13; // Arduino onboard LED; HIGH (= ON) while recording
#else
const int LED_OUT_R        = 22; // GR-KURUMI RGB-LED; active LOW
const int LED_OUT_G        = 23;
const int LED_OUT_B        = 24;
#endif
const int HBUSRDY          = A0;
const int PWRBTN           = A1; // Pulled up by camera
// END Don't change.
//                           A2; // (Not in use)
//                           A3; // (Not in use)
//                           A4; // (Arduino: Used by I2C SDA; GR-KURUMI: Not in use)
//                           A5; // (Arduino: Used by I2C SCL; GR-KURUMI: Not in use) 
//                           A6; // Analog only (Not in use)
const int LIGHT_SENSOR_PIN = A7; // Analog only

boolean lastHerobusState = LOW;  // Will be HIGH when camera attached.

// function prototypes
//   Arduino IDE doesn't need these prototypes but does Renesas Web Compiler
void resetI2C(void);
void setupShutter(void);
void setupSwitch(void);
void setupIRremote(void);
void setupLightSensor(void);
void setupPIRSensor(void);
void resetVMD(void);
void checkTimeAlarms(void);
void checkBacpacCommands(void);
void checkCameraCommands(void);
void checkSwitch(void);
void checkIRremote(void);
void checkLightSensor(void);
void checkPIRSensor(void);
void checkVMD(void);
// end of function prototypes

boolean ledState;

#ifndef REL_GR_KURUMI // Arduino
void ledOff()
{
  digitalWrite(LED_OUT, LOW);
  ledState = false;
}

void ledOn()
{
  digitalWrite(LED_OUT, HIGH);
  ledState = true;
}

void setupLED()
{
  pinMode(LED_OUT, OUTPUT);
  ledOff();
}
#else // GR-KURUMI
void ledOff()
{
  // white
  pinMode(LED_OUT_R, INPUT);
  pinMode(LED_OUT_G, INPUT);
  pinMode(LED_OUT_B, INPUT);
  ledState = false;
}

void ledOn()
{
  pinMode(LED_OUT_R, OUTPUT);
  pinMode(LED_OUT_G, OUTPUT);
  pinMode(LED_OUT_B, OUTPUT);
  // green
  digitalWrite(LED_OUT_R, HIGH);
  digitalWrite(LED_OUT_G, LOW);
  digitalWrite(LED_OUT_B, HIGH);
  ledState = true;
}

void setupLED()
{
  ledOff();
}
#endif

void setup() 
{
  // Remark. Arduino Pro Mini 328 3.3V 8MHz is too slow to catch up with the highest 115200 baud.
  //     cf. http://forum.arduino.cc/index.php?topic=54623.0
  // Set 57600 baud or slower.
  Serial.begin(57600);
  
  setupShutter();
  setupSwitch();
  setupIRremote();
  setupLightSensor();
  setupPIRSensor();

  setupLED(); // onboard LED setup 
  pinMode(BPRDY, OUTPUT); digitalWrite(BPRDY, LOW);    // Show camera MewPro attach. 
  pinMode(TRIG, OUTPUT); digitalWrite(TRIG, LOW);

  // don't forget to switch pin configurations to INPUT.
  pinMode(I2CINT, INPUT);  // Teensy: default disabled
  pinMode(HBUSRDY, INPUT); // default: analog input
  pinMode(PWRBTN, INPUT);  // default: analog input
}

void loop() 
{
  // Attach or detach bacpac
  if (digitalRead(HBUSRDY) == HIGH) {
    if (lastHerobusState != HIGH) {
      pinMode(I2CINT, OUTPUT); digitalWrite(I2CINT, HIGH);
      lastHerobusState = HIGH;
      resetI2C();
    }
  } else {
    if (lastHerobusState != LOW) {
      pinMode(I2CINT, INPUT);
      lastHerobusState = LOW;
    }
  }

  checkTimeAlarms();
  checkBacpacCommands();
  checkCameraCommands();
  checkSwitch();
  checkIRremote();
  checkLightSensor();
  checkPIRSensor();
  checkVMD();
}

