/**
   @file pressuresensor_i.cpp
   @brief Interface for PressureSensor

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
#include "pressuresensor_i.h"
#include "socketreader.h"

const char* PressureSensorChannelInterface::staticInterfaceName = "local.PressureSensor";

AbstractSensorChannelInterface* PressureSensorChannelInterface::factoryMethod(const QString& id, int sessionId)
{
    return new PressureSensorChannelInterface(OBJECT_PATH + "/" + id, sessionId);
}

PressureSensorChannelInterface::PressureSensorChannelInterface(const QString& path, int sessionId)
    : AbstractSensorChannelInterface(path, PressureSensorChannelInterface::staticInterfaceName, sessionId)
{
}

const PressureSensorChannelInterface* PressureSensorChannelInterface::listenInterface(const QString& id)
{
    return dynamic_cast<const PressureSensorChannelInterface*> (interface(id));
}

PressureSensorChannelInterface* PressureSensorChannelInterface::controlInterface(const QString& id)
{
    return interface(id);
}

PressureSensorChannelInterface* PressureSensorChannelInterface::interface(const QString& id)
{
    SensorManagerInterface& sm = SensorManagerInterface::instance();
    if (!sm.registeredAndCorrectClassName(id, PressureSensorChannelInterface::staticMetaObject.className())) {
        return nullptr;
    }

    return dynamic_cast<PressureSensorChannelInterface*>(sm.interface(id));
}

bool PressureSensorChannelInterface::dataReceivedImpl()
{
    QVector<TimedUnsigned> values;
    if (!read<TimedUnsigned>(values))
        return false;
    foreach (const TimedUnsigned& data, values)
        emit pressureChanged(data);
    return true;
}

Unsigned PressureSensorChannelInterface::pressure()
{
    return getAccessor<Unsigned>("pressure");
}
