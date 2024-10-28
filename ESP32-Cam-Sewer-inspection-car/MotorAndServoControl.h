/*
 * MotorAndServoControl.h
 *
 *  Copyright (C) 2021-2024  Armin Joachimsmeyer
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
 *
 */

#ifndef _MOTOR_AND_SERVO_CONTROL_H
#define _MOTOR_AND_SERVO_CONTROL_H

#include "PWMDcMotor.h"
extern PWMDcMotor DCMotor;
extern unsigned long sMillisOfLastAction;

void updateMotor();
void checkForAttention();
void setServoPan(int aNewDegree);
void initServoAndMotorPinsAndChannels(bool aIsAccesspoint);
bool ServoAndMotorCommandInterpreter(char *aCommandString, int aCommandValue);

#endif //#ifndef _MOTOR_AND_SERVO_CONTROL_H
