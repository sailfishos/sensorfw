/**
   @file stepcountersensor_i.cpp
   @brief Interface for stepcounter sensor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation
   Copyright (C) 2016 kimmoli

   @author Kimmo Lindholm <kimmo.lindholm@eke.fi>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
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

#include "sensormanagerinterface.h"
#include "stepcountersensor_i.h"
#include "socketreader.h"

const char* StepCounterSensorChannelInterface::staticInterfaceName = "local.StepCounterSensor";

AbstractSensorChannelInterface* StepCounterSensorChannelInterface::factoryMethod(const QString& id, int sessionId)
{
    return new StepCounterSensorChannelInterface(OBJECT_PATH + "/" + id, sessionId);
}

StepCounterSensorChannelInterface::StepCounterSensorChannelInterface(const QString& path, int sessionId)
    : AbstractSensorChannelInterface(path, StepCounterSensorChannelInterface::staticInterfaceName, sessionId)
{
}

const StepCounterSensorChannelInterface* StepCounterSensorChannelInterface::listenInterface(const QString& id)
{
    return dynamic_cast<const StepCounterSensorChannelInterface*> (interface(id));
}

StepCounterSensorChannelInterface* StepCounterSensorChannelInterface::controlInterface(const QString& id)
{
    return interface(id);
}

StepCounterSensorChannelInterface* StepCounterSensorChannelInterface::interface(const QString& id)
{
    SensorManagerInterface& sm = SensorManagerInterface::instance();
    if (!sm.registeredAndCorrectClassName(id, StepCounterSensorChannelInterface::staticMetaObject.className())) {
        return nullptr;
    }

    return dynamic_cast<StepCounterSensorChannelInterface*>(sm.interface(id));
}

bool StepCounterSensorChannelInterface::dataReceivedImpl()
{
    QVector<TimedUnsigned> values;
    if (!read<TimedUnsigned>(values))
        return false;
    foreach (const TimedUnsigned& data, values)
        emit StepCounterChanged(data);
    return true;
}

Unsigned StepCounterSensorChannelInterface::steps()
{
    return getAccessor<Unsigned>("steps");
}
