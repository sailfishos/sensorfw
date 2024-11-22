/**
   @file lidsensor_i.cpp
   @brief Interface for LidSensor

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

#include "sensormanagerinterface.h"
#include "lidsensor_i.h"
#include "socketreader.h"

const char* LidSensorChannelInterface::staticInterfaceName = "local.LidSensor";

AbstractSensorChannelInterface* LidSensorChannelInterface::factoryMethod(const QString& id, int sessionId)
{
    return new LidSensorChannelInterface(OBJECT_PATH + "/" + id, sessionId);
}

LidSensorChannelInterface::LidSensorChannelInterface(const QString& path, int sessionId)
    : AbstractSensorChannelInterface(path, LidSensorChannelInterface::staticInterfaceName, sessionId)
{
}

const LidSensorChannelInterface* LidSensorChannelInterface::listenInterface(const QString& id)
{
    return dynamic_cast<const LidSensorChannelInterface*> (interface(id));
}

LidSensorChannelInterface* LidSensorChannelInterface::controlInterface(const QString& id)
{
    return interface(id);
}

LidSensorChannelInterface* LidSensorChannelInterface::interface(const QString& id)
{
    SensorManagerInterface& sm = SensorManagerInterface::instance();
    if (!sm.registeredAndCorrectClassName(id, LidSensorChannelInterface::staticMetaObject.className())) {
        return nullptr;
    }

    return dynamic_cast<LidSensorChannelInterface*>(sm.interface(id));
}

bool LidSensorChannelInterface::dataReceivedImpl()
{
    QVector<LidData> values;
    if (!read<LidData>(values))
        return false;
    foreach (const LidData &data, values)
        emit lidChanged(data);
    return true;
}

LidData LidSensorChannelInterface::closed()
{
    return getAccessor<LidData>("closed");
}
