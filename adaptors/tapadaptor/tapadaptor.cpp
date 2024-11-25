/**
   @file tapadaptor.cpp
   @brief TapAdaptor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Matias Muhonen <ext-matias.muhonen@nokia.com>
   @author Lihan Guo <lihan.guo@digia.com>
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
#include "tapadaptor.h"
#include "config.h"
#include "datatypes/utils.h"

#include <errno.h>

TapAdaptor::TapAdaptor(const QString& id) :
    InputDevAdaptor(id, 1)
{
    tapBuffer_ = new DeviceAdaptorRingBuffer<TapData>(1);
    setAdaptedSensor("tap", "Internal accelerometer tap events", tapBuffer_);
    setDescription("Device tap events (lis302d)");
}

TapAdaptor::~TapAdaptor()
{
    delete tapBuffer_;
}

void TapAdaptor::interpretEvent(int src, struct input_event *ev)
{
    Q_UNUSED(src);

    if (ev->type == EV_KEY && ev->value == 1) {
        TapData::Direction dir;
        switch (ev->code) {
            case BTN_X:
                dir = TapData::X;
                break;
            case BTN_Y:
                dir = TapData::Y;
                break;
            case BTN_Z:
                dir = TapData::Z;
                break;
            default:
                dir = TapData::X;
                qCWarning(lcSensorFw) << id() << "TapAdaptor: Unknown event-code received: " << ev->code;
                break;
        }
        TapData tapValue;
        tapValue.direction_ = dir;
        tapValue.timestamp_ = Utils::getTimeStamp(ev);
        tapValue.type_ = TapData::SingleTap;

        commitOutput(tapValue);
    }
}

void TapAdaptor::interpretSync(int src, struct input_event *ev)
{
    Q_UNUSED(src);
    Q_UNUSED(ev);
}

void TapAdaptor::commitOutput(const TapData& data)
{
    TapData* d = tapBuffer_->nextSlot();

    d->timestamp_ = data.timestamp_;
    d->direction_ = data.direction_;
    d->type_ = data.type_;

    tapBuffer_->commit();
    tapBuffer_->wakeUpReaders();
}

bool TapAdaptor::setInterval(const int sessionId, const unsigned int interval_us)
{
    Q_UNUSED(sessionId);
    Q_UNUSED(interval_us);
    return true;
}
