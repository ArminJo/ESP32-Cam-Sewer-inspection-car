/*
 * MotorAndServoControl.h
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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/gpl.html>.
 *
 */

#ifndef MOTOR_AND_SERVO_CONTROL_HPP
#define MOTOR_AND_SERVO_CONTROL_HPP

#include "lib/PWMDcMotor/PWMDcMotor.h"
extern PWMDcMotor DCMotor;

void updateMotor();
void setServoPan(int aNewDegree);
void initServoAndMotorPinsAndChannels(bool aIsAccesspoint);
bool ServoAndMotorCommandInterpreter(char *aCommandString, int aCommandValue);

#endif //#ifndef MOTOR_AND_SERVO_CONTROL_HPP
#pragma once
