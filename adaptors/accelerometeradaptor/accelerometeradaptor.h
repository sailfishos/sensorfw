/**
   @file accelerometeradaptor.h
   @brief Contains AccelerometerAdaptor.

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   This file is part of Sensord.

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>

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

#ifndef ACCELEROMETERADAPTOR_H
#define ACCELEROMETERADAPTOR_H

#include "inputdevadaptor.h"
#include "sensord/deviceadaptorringbuffer.h"
#include "filters/orientationdata.h"
#include <QTime>

/**
 * @brief Adaptor for internal accelerometer.
 *
 * Adaptor for internal accelerometer. Uses SysFs driver interface in interval 
 * polling mode, i.e. values are read with given constant interval.
 *
 * Driver interface is located in @e /sys/class/i2c-adapter/i2c-3/3-001d/ .
 * <ul><li>@e coord filehandle provides measurement values.</li></ul>
 * No other filehandles are currently in use by this adaptor.
 *
 */
class AccelerometerAdaptor : public InputDevAdaptor
{
    Q_OBJECT;
public:
    /**
     * Factory method for gaining a new instance of AccelerometerAdaptor class.
     * @param id Identifier for the adaptor.
     */
    static DeviceAdaptor* factoryMethod(const QString& id)
    {
        return new AccelerometerAdaptor(id);
    }

protected:
    /**
     * Constructor.
     * @param id Identifier for the adaptor.
     */
    AccelerometerAdaptor(const QString& id);
    ~AccelerometerAdaptor();

private:
    DeviceAdaptorRingBuffer<OrientationData>* accelerometerBuffer_;
    OrientationData orientationValue_;
    QTime time;

    void interpretEvent(int src, struct input_event *ev);
    void commitOutput();
    void interpretSync(int src);
};

#endif