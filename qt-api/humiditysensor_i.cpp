/**
   @file humiditysensor_i.cpp
   @brief Interface for HumiditySensor

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
#include "humiditysensor_i.h"
#include "socketreader.h"

const char* HumiditySensorChannelInterface::staticInterfaceName = "local.HumiditySensor";

AbstractSensorChannelInterface* HumiditySensorChannelInterface::factoryMethod(const QString& id, int sessionId)
{
    return new HumiditySensorChannelInterface(OBJECT_PATH + "/" + id, sessionId);
}

HumiditySensorChannelInterface::HumiditySensorChannelInterface(const QString& path, int sessionId)
    : AbstractSensorChannelInterface(path, HumiditySensorChannelInterface::staticInterfaceName, sessionId)
{
}

const HumiditySensorChannelInterface* HumiditySensorChannelInterface::listenInterface(const QString& id)
{
    return dynamic_cast<const HumiditySensorChannelInterface*> (interface(id));
}

HumiditySensorChannelInterface* HumiditySensorChannelInterface::controlInterface(const QString& id)
{
    return interface(id);
}


HumiditySensorChannelInterface* HumiditySensorChannelInterface::interface(const QString& id)
{
    SensorManagerInterface& sm = SensorManagerInterface::instance();
    if (!sm.registeredAndCorrectClassName(id, HumiditySensorChannelInterface::staticMetaObject.className())) {
        return nullptr;
    }

    return dynamic_cast<HumiditySensorChannelInterface*>(sm.interface(id));
}

bool HumiditySensorChannelInterface::dataReceivedImpl()
{
    QVector<TimedUnsigned> values;
    if (!read<TimedUnsigned>(values))
        return false;
    foreach (const TimedUnsigned& data, values)
        emit relativeHumidityChanged(data);
    return true;
}

Unsigned HumiditySensorChannelInterface::relativeHumidity()
{
    return getAccessor<Unsigned>("relativeHumidity");
}
