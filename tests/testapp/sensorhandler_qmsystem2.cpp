/**
   @file sensorhandler_qmsystem2.cpp
   @brief sensor handler
   <p>
   Copyright (C) 2011 Nokia Corporation

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

#include "sensorhandler_qmsystem2.h"
#include "logging.h"

SensorHandler::SensorHandler(const QString& sensorName, QObject *parent) :
    AbstractSensorHandler(sensorName, parent),
    m_sensor(NULL)
{
    if (m_sensorName == "compasssensor")
    {
        m_sensor = new MeeGo::QmCompass();
        connect(m_sensor, SIGNAL(dataAvailable(const MeeGo::QmCompassReading)), this, SLOT(receivedData(const MeeGo::QmCompassReading)));
    }
    else if (m_sensorName == "magnetometersensor")
    {
        m_sensor = new MeeGo::QmMagnetometer();
        connect(m_sensor, SIGNAL(dataAvailable(const MeeGo::QmMagnetometerReading&)), this, SLOT(receivedData(const MeeGo::QmMagnetometerReading&)));
    }
    else if (m_sensorName ==  "orientationsensor")
    {
        m_sensor = new MeeGo::QmOrientation();
        connect(m_sensor, SIGNAL(orientationChanged(const MeeGo::QmOrientationReading)), this, SLOT(receivedData(const MeeGo::QmOrientationReading)));
    }
    else if (m_sensorName == "accelerometersensor")
    {
        m_sensor = new MeeGo::QmAccelerometer();
        connect(m_sensor, SIGNAL(dataAvailable(const MeeGo::QmAccelerometerReading&)), this, SLOT(receivedData(const MeeGo::QmAccelerometerReading&)));
    }
    else if (m_sensorName == "alssensor")
    {
        m_sensor = new MeeGo::QmALS();
        connect(m_sensor, SIGNAL(ALSChanged(const MeeGo::QmAlsReading)), this, SLOT(receivedData(const MeeGo::QmIntReading)));
    }
    else if (m_sensorName == "rotationsensor")
    {
        m_sensor = new MeeGo::QmRotation();
        connect(m_sensor, SIGNAL(dataAvailable(const MeeGo::QmRotationReading&)), this, SLOT(receivedData(const MeeGo::QmRotationReading&)));
    }
    else if (m_sensorName == "tapsensor")
    {
        m_sensor = new MeeGo::QmTap();
        connect(m_sensor, SIGNAL(tapped(const MeeGo::QmTapReading)), this, SLOT(receivedData(const MeeGo::QmTapReading)));
    }
    else if (m_sensorName == "proximitysensor")
    {
        m_sensor = new MeeGo::QmProximity();
        connect(m_sensor, SIGNAL(ProximityChanged(const MeeGo::QmProximityReading)), this, SLOT(receivedData(const MeeGo::QmIntReading)));
    }
}

void SensorHandler::receivedData(const MeeGo::QmAccelerometerReading& data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.x << " " << data.y << " " <<   data.z;
}

void SensorHandler::receivedData(const MeeGo::QmIntReading data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.value;
}

void SensorHandler::receivedData(const MeeGo::QmCompassReading data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.level << " " << data.degrees;
}

void SensorHandler::receivedData(const MeeGo::QmMagnetometerReading& data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.x << " " << data.y << " " << data.z << " "
                  << data.rx << " " << data.ry << " " << data.rz << " "
                  << data.level;
}

void SensorHandler::receivedData(const MeeGo::QmOrientationReading data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.value;
}

void SensorHandler::receivedData(const MeeGo::QmRotationReading& data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.x << " " << data.y << " " <<   data.z;
}

void SensorHandler::receivedData(const MeeGo::QmTapReading data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.type << " " << data.direction;
}

bool SensorHandler::startClient()
{
    m_sensor->requestSession(MeeGo::QmSensor::SessionTypeListen);
    m_sensor->setInterval(m_interval_ms);
    m_sensor->setStandbyOverride(m_standbyoverride);
    return true;
}

bool SensorHandler::init(const QStringList&)
{
    return true;
}

bool SensorHandler::stopClient()
{
    return true;
}
