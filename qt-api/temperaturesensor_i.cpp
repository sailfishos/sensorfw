/**
   @file temperaturesensor_i.cpp
   @brief Interface for TemperatureSensor

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

#include "sensormanagerinterface.h"
#include "temperaturesensor_i.h"
#include "socketreader.h"

const char* TemperatureSensorChannelInterface::staticInterfaceName = "local.TemperatureSensor";

AbstractSensorChannelInterface* TemperatureSensorChannelInterface::factoryMethod(const QString& id, int sessionId)
{
    return new TemperatureSensorChannelInterface(OBJECT_PATH + "/" + id, sessionId);
}

TemperatureSensorChannelInterface::TemperatureSensorChannelInterface(const QString& path, int sessionId)
    : AbstractSensorChannelInterface(path, TemperatureSensorChannelInterface::staticInterfaceName, sessionId)
{
}

const TemperatureSensorChannelInterface* TemperatureSensorChannelInterface::listenInterface(const QString& id)
{
    return dynamic_cast<const TemperatureSensorChannelInterface*> (interface(id));
}

TemperatureSensorChannelInterface* TemperatureSensorChannelInterface::controlInterface(const QString& id)
{
    return interface(id);
}

TemperatureSensorChannelInterface* TemperatureSensorChannelInterface::interface(const QString& id)
{
    SensorManagerInterface& sm = SensorManagerInterface::instance();
    if (!sm.registeredAndCorrectClassName(id, TemperatureSensorChannelInterface::staticMetaObject.className())) {
        return nullptr;
    }

    return dynamic_cast<TemperatureSensorChannelInterface*>(sm.interface(id));
}

bool TemperatureSensorChannelInterface::dataReceivedImpl()
{
    QVector<TimedUnsigned> values;
    if (!read<TimedUnsigned>(values))
        return false;
    foreach (const TimedUnsigned& data, values)
        emit temperatureChanged(data);
    return true;
}

Unsigned TemperatureSensorChannelInterface::temperature()
{
    return getAccessor<Unsigned>("temperature");
}
