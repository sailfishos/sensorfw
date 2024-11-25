/**
   @file sensormanager_a.cpp
   @brief D-Bus adaptor for SensorManager

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Joep van Gassel <joep.van.gassel@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Semi Malinen <semi.malinen@nokia.com
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

#include "sensormanager_a.h"
#include "logging.h"

/*
 * Implementation of adaptor class SensorManagerAdaptor
 */
SensorManagerAdaptor::SensorManagerAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);//be sure to send error signals on
}

int SensorManagerAdaptor::errorCodeInt() const
{
    return sensorManager()->errorCodeInt();
}

QString SensorManagerAdaptor::errorString() const
{
    return sensorManager()->errorString();
}

bool SensorManagerAdaptor::loadPlugin(const QString& name)
{
    return sensorManager()->loadPlugin(name);
}

QStringList SensorManagerAdaptor::availablePlugins() const
{
    return sensorManager()->availablePlugins();
}

bool SensorManagerAdaptor::pluginAvailable(const QString &name) const
{
    return sensorManager()->pluginAvailable(name);
}

QStringList SensorManagerAdaptor::availableSensorPlugins() const
{
    return sensorManager()->availableSensorPlugins();
}

int SensorManagerAdaptor::requestSensor(const QString &id, qint64 pid)
{
    int session = sensorManager()->requestSensor(id);
    qCInfo(lcSensorFw) << "Sensor '" << id << "' requested. Created session: " << session << ". Client PID: " << pid;
    return session;
}

bool SensorManagerAdaptor::releaseSensor(const QString &id, int sessionId, qint64 pid)
{
    qCInfo(lcSensorFw) << "Sensor '" << id << "' release requested for session " << sessionId << ". Client PID: " << pid;
    return sensorManager()->releaseSensor(id, sessionId);
}

void SensorManagerAdaptor::setMagneticDeviation(double level)
{
    sensorManager()->setMagneticDeviation(level);
}

double SensorManagerAdaptor::magneticDeviation()
{
    return sensorManager()->magneticDeviation();
}

SensorManager* SensorManagerAdaptor::sensorManager() const
{
    return dynamic_cast<SensorManager*>(parent());
}
