/**
   @file compasssensor_i.cpp
   @brief Interface for CompassSensor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

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
#include "compasssensor_i.h"

const char* CompassSensorChannelInterface::staticInterfaceName = "local.CompassSensor";

AbstractSensorChannelInterface* CompassSensorChannelInterface::factoryMethod(const QString& id, int sessionId)
{
    return new CompassSensorChannelInterface(OBJECT_PATH + "/" + id, sessionId);
}

CompassSensorChannelInterface::CompassSensorChannelInterface(const QString &path, int sessionId)
    : AbstractSensorChannelInterface(path, CompassSensorChannelInterface::staticInterfaceName, sessionId)
    , useDeclination_(true)
{
}

const CompassSensorChannelInterface* CompassSensorChannelInterface::listenInterface(const QString& id)
{
    return dynamic_cast<const CompassSensorChannelInterface*>(interface(id));
}

CompassSensorChannelInterface* CompassSensorChannelInterface::controlInterface(const QString& id)
{
    return interface(id);
}

CompassSensorChannelInterface* CompassSensorChannelInterface::interface(const QString& id)
{
    SensorManagerInterface& sm = SensorManagerInterface::instance();
    if (!sm.registeredAndCorrectClassName(id, CompassSensorChannelInterface::staticMetaObject.className())) {
        return 0;
    }

    return dynamic_cast<CompassSensorChannelInterface*>(sm.interface(id));
}

bool CompassSensorChannelInterface::dataReceivedImpl()
{
    QVector<CompassData> values;
    if (!read<CompassData>(values))
        return false;
    foreach (const CompassData& data, values)
        emit dataAvailable(Compass(data, useDeclination_));
    return true;
}

Compass CompassSensorChannelInterface::get()
{
    return Compass(getAccessor<Compass>("value").data(), useDeclination_);
}

bool CompassSensorChannelInterface::useDeclination()
{
    return useDeclination_;
}

void CompassSensorChannelInterface::setUseDeclination(bool enable)
{
    useDeclination_ = enable;
}

int CompassSensorChannelInterface::declinationValue()
{
    return getAccessor<int>("declinationValue");
}
