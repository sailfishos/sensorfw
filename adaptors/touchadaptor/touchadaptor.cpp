/**
   @file touchadaptor.cpp
   @brief TouchAdaptor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
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

#include "touchadaptor.h"
#include "datatypes/utils.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

const int TouchAdaptor::HARD_MAX_TOUCH_POINTS = 5;

TouchAdaptor::TouchAdaptor(const QString& id) : InputDevAdaptor(id, HARD_MAX_TOUCH_POINTS)
{
    outputBuffer_ = new DeviceAdaptorRingBuffer<TouchData>(1);
    setAdaptedSensor("touch", "Touch screen input", outputBuffer_);
    setDescription("Touch screen events");
}

TouchAdaptor::~TouchAdaptor()
{
    delete outputBuffer_;
}

bool TouchAdaptor::checkInputDevice(QString path, QString matchString)
{
    Q_UNUSED(matchString);

    int fd;
    char deviceName[256];
    unsigned char evtype_bitmask[20+1];

    fd = open(path.toLocal8Bit().constData(), O_RDONLY);
    if (fd == -1) {
        return false;
    }

    ioctl(fd, EVIOCGNAME(sizeof(deviceName)), deviceName);

    if (ioctl(fd, EVIOCGBIT(0, sizeof(evtype_bitmask)), evtype_bitmask) < 0) {
        qCWarning(lcSensorFw) << id() << __PRETTY_FUNCTION__ << deviceName << "EVIOCGBIT error";
        close(fd);
        return false;
    }

#define test_bit(bit, array)    (array[bit/8] & (1<<(bit%8)))

    if (!test_bit(EV_ABS, evtype_bitmask)) {
        close(fd);
        return false;
    }

    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(evtype_bitmask)), evtype_bitmask) < 0) {
        qCWarning(lcSensorFw) << id() << __PRETTY_FUNCTION__ << deviceName << "EVIOGBIT EV_ABS error.";
        close(fd);
        return false;
    }

    if (!test_bit(ABS_X, evtype_bitmask) || !test_bit(ABS_Y, evtype_bitmask)) {
        qCWarning(lcSensorFw) << id() << __PRETTY_FUNCTION__ << deviceName << "Testbit ABS_X or ABS_Y failed.";
        close(fd);
        return false;
    }

#undef test_bit

    // Final range is defined by the last found event handle.
    // Make separate for each input device if necessary.
    struct input_absinfo info;
    ioctl(fd, EVIOCGABS(ABS_X), &info);
    rangeInfo_.xMin = info.minimum;
    rangeInfo_.xRange = info.maximum - info.minimum;

    ioctl(fd, EVIOCGABS(ABS_Y), &info);
    rangeInfo_.yMin = info.minimum;
    rangeInfo_.yRange = info.maximum - info.minimum;

    close(fd);

    return true;
}

void TouchAdaptor::interpretEvent(int src, struct input_event *ev)
{
    switch (ev->type) {

        case EV_SYN:
            commitOutput(src, ev);
            break;

        case EV_ABS:
            switch (ev->code) {
                case ABS_X:
                    touchValues_[src].x = ev->value;
                    break;
                case ABS_Y:
                    touchValues_[src].y = ev->value;
                    break;
                case ABS_Z:
                    touchValues_[src].z = ev->value;
                    break;
            }
            break;

        case EV_KEY:
            switch (ev->code) {
                case BTN_TOUCH:
                    if (ev->value) {
                        touchValues_[src].fingerState = TouchData::FingerStateAccurate;
                    } else {
                        touchValues_[src].fingerState = TouchData::FingerStateNotPresent;
                    }
                    break;

                case BTN_MODE:
                    if (ev->value && touchValues_[src].fingerState!=TouchData::FingerStateNotPresent) {
                        touchValues_[src].fingerState = TouchData::FingerStateInaccurate;
                    }
                    break;
            }
            break;
    }
}

void TouchAdaptor::interpretSync(int src, struct input_event *ev)
{
    commitOutput(src, ev);
}

void TouchAdaptor::commitOutput(int src, struct input_event *ev)
{
    TouchData* d = outputBuffer_->nextSlot();

    d->timestamp_ = Utils::getTimeStamp(ev);
    d->x_ = touchValues_[src].x;
    d->y_ = touchValues_[src].y;
    d->z_ = touchValues_[src].z;
    d->object_ = src;
    d->state_ = touchValues_[src].fingerState;

    outputBuffer_->commit();
    outputBuffer_->wakeUpReaders();
}
