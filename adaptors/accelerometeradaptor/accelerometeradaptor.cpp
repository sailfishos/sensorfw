/**
   @file accelerometeradaptor.cpp
   @brief Contains AccelerometerAdaptor.

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Antti Virtanen <antti.i.virtanen@nokia.com>
   @author Shenghua <ext-shenghua.1.liu@nokia.com>

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


#include "accelerometeradaptor.h"

#include "logging.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <QMap>
#include "config.h"

#include "datatypes/utils.h"

AccelerometerAdaptor::AccelerometerAdaptor(const QString& id) :
    InputDevAdaptor(id, 1)
{
    accelerometerBuffer_ = new DeviceAdaptorRingBuffer<OrientationData>(1);
    setAdaptedSensor("accelerometer", "Internal accelerometer coordinates", accelerometerBuffer_);
    setDescription("Input device accelerometer adaptor");
    powerStatePath_ = SensorFrameworkConfig::configuration()->value("accelerometer/powerstate_path").toByteArray();
    accelMultiplier = SensorFrameworkConfig::configuration()->value("accelerometer/multiplier", QVariant(1)).toReal();
}

AccelerometerAdaptor::~AccelerometerAdaptor()
{
    stopSensor();
    delete accelerometerBuffer_;
}

bool AccelerometerAdaptor::startSensor()
{
    if (!powerStatePath_.isEmpty()) {
        writeToFile(powerStatePath_, "1");
    }
    return SysfsAdaptor::startSensor();
}

void AccelerometerAdaptor::stopSensor()
{
    if (!powerStatePath_.isEmpty()) {
        writeToFile(powerStatePath_, "0");
    }
    SysfsAdaptor::stopSensor();
}

void AccelerometerAdaptor::interpretEvent(int src, struct input_event *ev)
{
    Q_UNUSED(src);

    switch (ev->type) {
    case EV_REL:
    case EV_ABS:
        switch (ev->code) {
        case ABS_X:
            orientationValue_.x_ = ev->value * accelMultiplier;
            break;
        case ABS_Y:
            orientationValue_.y_ = ev->value * accelMultiplier;
            break;
        case ABS_Z:
            orientationValue_.z_ = ev->value * accelMultiplier;
            break;
        }
        break;
    }
}

void AccelerometerAdaptor::interpretSync(int src, struct input_event *ev)
{
    Q_UNUSED(src);
    commitOutput(ev);
}

void AccelerometerAdaptor::commitOutput(struct input_event *ev)
{
    AccelerationData* d = accelerometerBuffer_->nextSlot();

    d->timestamp_ = Utils::getTimeStamp(ev);
    d->x_ = orientationValue_.x_;
    d->y_ = orientationValue_.y_;
    d->z_ = orientationValue_.z_;

//    sensordLogT() << id() << "Accelerometer reading: " << d->x_ << ", " << d->y_ << ", " << d->z_;

    accelerometerBuffer_->commit();
    accelerometerBuffer_->wakeUpReaders();
}

bool AccelerometerAdaptor::resume()
{
   startSensor();
   return true;
}

bool AccelerometerAdaptor::standby()
{
    stopSensor();
    return true;
}
