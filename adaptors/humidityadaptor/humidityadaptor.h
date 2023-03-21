/**
   @file humidityadaptor.h
   @brief Contains HumidityAdaptor

   <p>
   Copyright (C) 2016 Canonical LTD.

   @author Lorn Potter <lorn.potter@canonical.com>

   This file is part of Sensorfw.

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
#ifndef HUMIDITYADAPTOR_H
#define HUMIDITYADAPTOR_H

#include "inputdevadaptor.h"
#include "deviceadaptorringbuffer.h"
#include "datatypes/orientationdata.h"
#include <QTime>

class HumidityAdaptor : public InputDevAdaptor
{
    Q_OBJECT
public:
    /**
     * Factory method for gaining a new instance of AccelerometerAdaptor class.
     * @param id Identifier for the adaptor.
     */
    static DeviceAdaptor* factoryMethod(const QString& id)
    {
        return new HumidityAdaptor(id);
    }

    virtual bool startSensor();

    virtual void stopSensor();

    virtual bool standby();

    virtual bool resume();

protected:
    /**
     * Constructor.
     * @param id Identifier for the adaptor.
     */
    HumidityAdaptor(const QString& id);
    ~HumidityAdaptor();

private:
    DeviceAdaptorRingBuffer<TimedUnsigned>* humidityBuffer_;

    unsigned humidityValue_;

    void interpretEvent(int src, struct input_event *ev);
    void commitOutput(struct input_event *ev);
    void interpretSync(int src, struct input_event *ev);
    QByteArray powerStatePath_;

};

#endif
