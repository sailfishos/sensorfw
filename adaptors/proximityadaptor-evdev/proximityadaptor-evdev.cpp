/**
   @file proximityadaptor-evdev.cpp
   @brief ProximityAdaptor utilizing evdev kernel interface

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Markus Lehtonen <markus.lehtonen@nokia.com>
   @author Antti Virtanen <antti.i.virtanen@nokia.com>

   This file is part of Sensord.

   Sensord is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License
   version 2.1 as published by the Free Software Foundation.

   Sensord is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Sensord.  If not, see <http://www.gnu.org/licenses/>.
   </p>
*/

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <string.h>
#include <logging.h>
#include "datatypes/utils.h"
#include "config.h"

#include "proximityadaptor-evdev.h"

#define SELFDEF_EV_SW 5
#define SELFDEF_EV_FRONT_PROXIMITY 11
#define SELFDEF_EV_DISTANCE 25

ProximityAdaptorEvdev::ProximityAdaptorEvdev(const QString& id) :
    InputDevAdaptor(id, 1),
    currentState_(ProximityStateUnknown)
{
    proximityBuffer_ = new DeviceAdaptorRingBuffer<ProximityData>(1);
    setAdaptedSensor("proximity", "Proximity state", proximityBuffer_);
    powerStatePath_ = SensorFrameworkConfig::configuration()->value("proximity/powerstate_path").toByteArray();
}

ProximityAdaptorEvdev::~ProximityAdaptorEvdev()
{
    delete proximityBuffer_;
}

void ProximityAdaptorEvdev::interpretEvent(int src, struct input_event *ev)
{
    Q_UNUSED(src);
    if ((ev->type == EV_SW && ev->code == SW_FRONT_PROXIMITY) ||
            (ev->type == EV_ABS && ev->code == ABS_DISTANCE)) {
        if (ev->value == 0) {
            currentState_ = ProximityStateClosed;
        } else if (ev->value == 1) {
            currentState_ = ProximityStateOpen;
        } else {
            currentState_ = ProximityStateUnknown;
        }
    }
}

void ProximityAdaptorEvdev::interpretSync(int src, struct input_event *ev)
{
    Q_UNUSED(src);
    commitOutput(ev);
}

void ProximityAdaptorEvdev::commitOutput(struct input_event *ev)
{
    static ProximityState oldState = ProximityStateUnknown;

    if (currentState_ != oldState) {
        qCInfo(lcSensorFw) << id() << "Proximity state change detected: " << currentState_;

        ProximityData *proximityData = proximityBuffer_->nextSlot();

        proximityData->timestamp_ = Utils::getTimeStamp(ev);
        proximityData->withinProximity_ = currentState_;

        oldState = currentState_;

        proximityBuffer_->commit();
        proximityBuffer_->wakeUpReaders();
    }
}

bool ProximityAdaptorEvdev::startSensor()
{
    if (!powerStatePath_.isEmpty()) {
        writeToFile(powerStatePath_, "1");
    }
    if (SysfsAdaptor::startSensor()) {
        return true;
    }
    return false;
}

void ProximityAdaptorEvdev::stopSensor()
{
    if (!powerStatePath_.isEmpty()) {
        writeToFile(powerStatePath_, "0");
    }

    SysfsAdaptor::stopSensor();
}

bool ProximityAdaptorEvdev::standby()
{
    stopSensor();
    return true;
}

bool ProximityAdaptorEvdev::resume()
{
    startSensor();
    return true;
}
