/**
   @file lidsensoradaptor-evdev.cpp
   @brief LidSensorAdaptor utilizing evdev kernel interface

   <p>
   Copyright (C) 2016 Canonical,  Ltd.

   @author Lorn Potter <lorn.potter@canonical.com>

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

#include "lidsensoradaptor-evdev.h"

#define SELFDEF_SW_LID 0
#define SELFDEF_EV_SW 5
#define SELFDEF_EV_DISTANCE 25

LidSensorAdaptorEvdev::LidSensorAdaptorEvdev(const QString& id) :
    InputDevAdaptor(id, 2),
    currentType_(LidData::UnknownLid),
    lastType(LidData::UnknownLid),
    currentValue_(-1),
    lastValue(-1),
    usingFront(false)
{
    lidBuffer_ = new DeviceAdaptorRingBuffer<LidData>(1);
    setAdaptedSensor("lidsensor", "Lid state", lidBuffer_);
    powerStatePath_ = SensorFrameworkConfig::configuration()->value("lidsensor/powerstate_path").toByteArray();

}

LidSensorAdaptorEvdev::~LidSensorAdaptorEvdev()
{
    delete lidBuffer_;
}

/* Dell  event0 Lid Switch
 * type EV_SW code EV_LID value 1 closed
 * */

/* Dell event8 Virtual Button Driver (happens with front lid too)
 * back closed:
 * type (4) EV_MSC code (4) MSC_SCAN value cb
 * EV_MSC MSC_SCAN value cc
 *
 * back open:
 * EV_MSC MSC_SCAN value ca
 * EV_MSC MSC_SCAN value cd
 *
 *
 * */

// there can be two lid sensors, so try and use both
void LidSensorAdaptorEvdev::init()
{
    qDebug() << id() << Q_FUNC_INFO << name();
    QStringList strList = SensorFrameworkConfig::configuration()->value<QStringList>(name() + "/input_match");
    qDebug() << id() << strList;
    foreach (const QString str, strList) {
        if (!getInputDevices(str)) {
            sensordLogW() << id() << "Input device not found.";
            SysfsAdaptor::init();
        }
    }
}

void LidSensorAdaptorEvdev::interpretEvent(int src, struct input_event *ev)
{
    Q_UNUSED(src);

    if (ev->type == SELFDEF_EV_SW && ev->code == SELFDEF_SW_LID) {
        currentValue_ = ev->value;
        currentType_ = LidData::FrontLid;
        if (currentValue_ == 1)
            usingFront = true;
        else
            usingFront = false;
    }
    if (ev->type == EV_MSC && ev->code == MSC_SCAN && !usingFront) {

        switch (ev->value) {
        case 0xCC: // closed
            currentValue_ = 1;
            break;
        case 0xCD: // open
            currentValue_ = 0;
            break;
        };
        currentType_ = LidData::BackLid;
    }
}

void LidSensorAdaptorEvdev::interpretSync(int /*src*/, struct input_event *ev)
{
    commitOutput(ev);
}

void LidSensorAdaptorEvdev::commitOutput(struct input_event *ev)
{
    if (lastValue != currentValue_
            && ((currentType_ == LidData::FrontLid)
                || (!usingFront && currentType_ == LidData::BackLid))) {

        LidData *lidData = lidBuffer_->nextSlot();

        lidData->timestamp_ = Utils::getTimeStamp(ev);
        lidData->value_ = currentValue_;
        lidData->type_ = currentType_;
        sensordLogD() << id() << "Lid state change detected: "
                      << (currentType_ == 0 ? "front" : "back")
                      << (currentValue_ == 0 ? "OPEN": "CLOSED");

        lidBuffer_->commit();
        lidBuffer_->wakeUpReaders();
        lastValue = currentValue_;
        lastType = currentType_;
    }
}

bool LidSensorAdaptorEvdev::startSensor()
{
    if (!powerStatePath_.isEmpty()) {
        //   writeToFile(powerStatePath_, "1");
    }
    if (SysfsAdaptor::startSensor()) {
        return true;
    }
    return false;
}

void LidSensorAdaptorEvdev::stopSensor()
{
    if (!powerStatePath_.isEmpty()) {
        //  writeToFile(powerStatePath_, "0");
    }

    SysfsAdaptor::stopSensor();
}

bool LidSensorAdaptorEvdev::standby()
{
    stopSensor();
    return true;
}

bool LidSensorAdaptorEvdev::resume()
{
    startSensor();
    return true;
}
