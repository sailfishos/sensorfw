/**
   @file rotationsensor_i.cpp
   @brief Interface for RotationSensor

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
#include "rotationsensor_i.h"

const char* RotationSensorChannelInterface::staticInterfaceName = "local.RotationSensor";

AbstractSensorChannelInterface* RotationSensorChannelInterface::factoryMethod(const QString& id, int sessionId)
{
    return new RotationSensorChannelInterface(OBJECT_PATH + "/" + id, sessionId);
}

RotationSensorChannelInterface::RotationSensorChannelInterface(const QString &path, int sessionId)
    : AbstractSensorChannelInterface(path, RotationSensorChannelInterface::staticInterfaceName, sessionId)
    , frameAvailableConnected(false)
{
}

const RotationSensorChannelInterface* RotationSensorChannelInterface::listenInterface(const QString& id)
{
    return dynamic_cast<const RotationSensorChannelInterface*>(interface(id));
}

RotationSensorChannelInterface* RotationSensorChannelInterface::controlInterface(const QString& id)
{
    return interface(id);
}

RotationSensorChannelInterface* RotationSensorChannelInterface::interface(const QString& id)
{
    SensorManagerInterface& sm = SensorManagerInterface::instance();
    if (!sm.registeredAndCorrectClassName(id, RotationSensorChannelInterface::staticMetaObject.className())) {
        return nullptr;
    }

    return dynamic_cast<RotationSensorChannelInterface*>(sm.interface(id));
}

bool RotationSensorChannelInterface::dataReceivedImpl()
{
    QVector<TimedXyzData> values;
    if (!read<TimedXyzData>(values))
        return false;
    if (!frameAvailableConnected || values.size() == 1) {
        foreach (const TimedXyzData& data, values)
            emit dataAvailable(XYZ(data));
    } else {
        QVector<XYZ> realValues;
        realValues.reserve(values.size());
        foreach (const TimedXyzData& data, values)
            realValues.push_back(XYZ(data));
        emit frameAvailable(realValues);
    }
    return true;
}

XYZ RotationSensorChannelInterface::rotation()
{
    return getAccessor<XYZ>("rotation");
}

bool RotationSensorChannelInterface::hasZ()
{
    return getAccessor<bool>("hasZ");
}

void RotationSensorChannelInterface::connectNotify(const QMetaMethod &signal)
{
    static const QMetaMethod frameAvailableSignal = QMetaMethod::fromSignal(&RotationSensorChannelInterface::frameAvailable);
    if (signal == frameAvailableSignal)
        frameAvailableConnected = true;
    dbusConnectNotify(signal);
}
