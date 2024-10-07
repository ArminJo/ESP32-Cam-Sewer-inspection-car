/*
 * MotorAndServoControl.cpp
 *
 *  Contains code for contol of the pn servo and the motor of the sewer inspection robot car.
 *  Requires the PWMDcMotor library.
 *
 *
 *  Copyright (C) 2021  Armin Joachimsmeyer
 *  armin.joachimsmeyer@gmail.com
 *
 *  This file is part of ServoEasing https://github.com/ArminJo/ServoEasing.
 *
 *  ServoEasing is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/gpl.html>.
 */
#include <Arduino.h>
#include "hal/ledc_types.h"

/*
 * You will need to change these values according to your motor, H-bridge and motor supply voltage.
 * You must specify this before the include of "CarPWMMotorControl.hpp"
 */
//#define USE_ADAFRUIT_MOTOR_SHIELD  // Activate this if you use Adafruit Motor Shield v2 connected by I2C instead of TB6612 or L298 breakout board.
//#define USE_STANDARD_LIBRARY_FOR_ADAFRUIT_MOTOR_SHIELD  // Activate this to force using of Adafruit library. Requires 694 bytes program memory.
//#define VIN_2_LIPO                 // Activate this, if you use 2 LiPo Cells (around 7.4 volt) as Motor supply.
#define VIN_1_LIPO                 // Or if you use a Mosfet bridge, 1 LIPO (around 3.7 volt) may be sufficient.
//#define FULL_BRIDGE_INPUT_MILLIVOLT   6000  // Default. For 4 x AA batteries (6 volt).
#define MOSFET_BRIDGE_USED  // Activate this, if you use a (recommended) mosfet bridge instead of a L298 bridge, which has higher losses.
//#define DEFAULT_DRIVE_MILLIVOLT       2000 // Drive voltage -motors default speed- is 2.0 volt
//#define DO_NOT_SUPPORT_RAMP  // Ramps are anyway not used if drive speed voltage (default 2.0 V) is below 2.3 V. Saves 378 bytes program space.
#include "PWMDcMotor.hpp" // include the sources of the PWMDcMotor library

#include "MotorAndServoControl.h"

PWMDcMotor DCMotor;

int ServoPanDegree = 90; //default to front
int LastMotorSpeed = 128; //default 50%

#define MILLIS_OF_INACTIVITY_BEFORE_REMINDER_MOVE 180000 // 3 Minutes
#define MILLIS_OF_INACTIVITY_BETWEEN_REMINDER_MOVE 120000 // 2 Minutes
unsigned long sMillisOfLastAction;

// Defaults from Arduino Servo.h
#define DEFAULT_uS_LOW      544
#define DEFAULT_uS_HIGH     2400

/*
 * Pin assignment
 */
#define PAN_SERVO_CHANNEL   LEDC_CHANNEL_2
#define PAN_SERVO_PIN       12

#define DC_MOTOR_FORWARD_PIN    14
#define DC_MOTOR_BACKWARD_PIN   15
#define DC_MOTOR_SPEED_CHANNEL  LEDC_CHANNEL_4
#define DC_MOTOR_SPEED_PIN      13

//int ServoTiltDegree = 45; //default to horizontal
//#define TILT_SERVO_CHANNEL  LEDC_CHANNEL_3
//#define TILT_SERVO_PIN      2

#if !defined(ESP_ARDUINO_VERSION)
#define ESP_ARDUINO_VERSION 0
#endif
#if !defined(ESP_ARDUINO_VERSION_VAL)
#define ESP_ARDUINO_VERSION_VAL(major, minor, patch) 202
#endif

void initAnalogWrite() {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    analogWriteResolution(DC_MOTOR_SPEED_PIN, 8); // 3.x API
    analogWriteFrequency(DC_MOTOR_SPEED_PIN, 500); // 3.x API
#else
    ledcSetup(DC_MOTOR_SPEED_CHANNEL, 500, 8); //channel, 500 Hz, 8 bit resolution (for 2 ms)
    ledcAttachPin(DC_MOTOR_SPEED_PIN, DC_MOTOR_SPEED_CHANNEL); // pin, channel
#endif
}

void analogWrite(int8_t pin, int aSpeed) {
    ledcWrite(DC_MOTOR_SPEED_PIN, aSpeed);
    sMillisOfLastAction = millis();
}

// Arduino like analogWrite
void ledcServoWrite(uint8_t channel, uint32_t aDegree) {
    // Servo is head down!
    int tMicroseconds = map(aDegree, 180, 0, DEFAULT_uS_LOW, DEFAULT_uS_HIGH);
    // Microseconds to ticks 2^16 * 50 ticks / s
    int tTicks = (tMicroseconds * 65536) / 20000;

//    Serial.print("ticks=");
//    Serial.println(tTicks);
    ledcWrite(channel, tTicks);
    sMillisOfLastAction = millis();
}

void doAttention() {
    int tOriginalServoPanDegree = ServoPanDegree;
    setServoPan(70);
    delay(400);
    setServoPan(110);
    delay(400);
    setServoPan(tOriginalServoPanDegree);
}

void initServoAndMotorPinsAndChannels(bool aIsAccesspoint) {
    /*
     * Servo
     */
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcAttach(PAN_SERVO_PIN, 50, 16); // New API
#else
    ledcSetup(PAN_SERVO_CHANNEL, 50, 16); //channel, 50 Hz, 16 bit resolution (for 20 ms)
    ledcAttachPin(PAN_SERVO_PIN, PAN_SERVO_CHANNEL); // pin, channel
#endif
    ledcServoWrite(PAN_SERVO_CHANNEL, ServoPanDegree);
    //    ledcSetup(TILT_SERVO_CHANNEL, 50, 16);
    //    ledcAttachPin(TILT_SERVO_PIN, TILT_SERVO_CHANNEL);
    //    ledcServoWrite(TILT_SERVO_CHANNEL, ServoTiltDegree);

    // Feedback
    delay(400);
    doAttention();
    delay(1000);

    /*
     * Motor
     */
    initAnalogWrite();
    DCMotor.init(DC_MOTOR_FORWARD_PIN, DC_MOTOR_BACKWARD_PIN, DC_MOTOR_SPEED_PIN);
    // Feedback - go forward for (standalone) access point mode and backward for (known) network mode
//    PWMDcMotor::printSettings(&Serial);
    if (aIsAccesspoint) {
        DCMotor.goDistanceMillimeter(20, DIRECTION_BACKWARD);
    } else {
        DCMotor.goDistanceMillimeter(20, DIRECTION_FORWARD);
    }
    DCMotor.setDriveSpeedPWM(128);

}

void updateMotor() {
    DCMotor.updateMotor();
    if (millis() - sMillisOfLastAction > MILLIS_OF_INACTIVITY_BEFORE_REMINDER_MOVE) {
        sMillisOfLastAction += MILLIS_OF_INACTIVITY_BETWEEN_REMINDER_MOVE;
        doAttention();
        // next attention in 1 minute
    }
    LastMotorSpeed = DCMotor.DriveSpeedPWM;
}

void setServoPan(int aNewDegree) {
    if (aNewDegree != -1) {
        aNewDegree = constrain(aNewDegree, 0, 180);
        ServoPanDegree = aNewDegree;
        ledcServoWrite(PAN_SERVO_CHANNEL, aNewDegree); // channel, value
        Serial.print("Servo pan: ");
        Serial.print(aNewDegree);
        Serial.println(" degree");
    }
}

bool ServoAndMotorCommandInterpreter(char *aCommandString, int aCommandValue) {
    if (!strcmp(aCommandString, "pan")) {
        setServoPan(aCommandValue);
    } else if (!strcmp(aCommandString, "motor-speed")) {
        DCMotor.updateDriveSpeedPWM(aCommandValue);
        Serial.print("Speed: ");
        Serial.println(DCMotor.DriveSpeedPWM);
    } else if (!strcmp(aCommandString, "move-car")) {
        DCMotor.startGoDistanceMillimeter(aCommandValue * 10); // *10 since aCommandValue is cm
        Serial.print("Start go distance millimeter: ");
        Serial.println(aCommandValue * 10);
    } else {
        return false;
    }
    return true;
}

//void setServoTilt(int aNewDegree) {
//    if (aNewDegree != -1) {
//        aNewDegree = constrain(aNewDegree, 0, 180);
//        ServoTiltDegree = aNewDegree;
//        ledcServoWrite(TILT_SERVO_CHANNEL, aNewDegree);
//        Serial.print("Servo tilt: ");
//        Serial.print(aNewDegree);
//        Serial.println(" degree");
//    }
//}
