/**
   @file sensorhandler_qtapi.cpp
   @brief sensor handler
   <p>
   Copyright (C) 2010-2011 Nokia Corporation

   @author Shenghua Liu <ext-shenghua.1.liu@nokia.com>
   @author Lihan Guo <ext-lihan.4.guo@nokia.com>
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

#include "sensorhandler_qtapi.h"
#include "logging.h"

SensorHandler::SensorHandler(const QString& sensorName, QObject *parent) :
    AbstractSensorHandler(sensorName, parent),
    m_sensorChannelInterface(NULL)
{
}

void SensorHandler::receivedData(const MagneticField& data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.x() << " " << data.y() << " " <<   data.z();
}

void SensorHandler::receivedData(const XYZ& data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.x() << " " << data.y() << " " <<   data.z();
}

void SensorHandler::receivedData(const Compass& data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.degrees() << " " << data.level();
}

void SensorHandler::receivedData(const Unsigned& data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.x();
}

void SensorHandler::receivedData(const Tap& data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.direction() << " " << data.type();
}

void SensorHandler::receivedData(const Proximity& data)
{
    ++m_dataCount;
    sensordLogT() << m_sensorName << " sample " << m_dataCount << ": "
                  << data.UnsignedData().value_ << " " << data.proximityData().value_;
}

void SensorHandler::receivedFrame(const QVector<MagneticField>& frame)
{
    ++m_frameCount;
    sensordLogT() << m_sensorName << " frame " << m_frameCount << " size " << frame.size();
    foreach (const MagneticField& data, frame)
    {
        sensordLogT() << data.x() << " " << data.y() << " " << data.z();
        ++m_dataCount;
    }
}

void SensorHandler::receivedFrame(const QVector<XYZ>& frame)
{
    ++m_frameCount;
    sensordLogT() << m_sensorName << " frame " << m_frameCount << " size " << frame.size();
    foreach (const XYZ& data, frame)
    {
        sensordLogT() << data.x() << " " << data.y() << " " << data.z();
        ++m_dataCount;
    }
}

bool SensorHandler::startClient()
{
    createSensorInterface();
    if (m_sensorChannelInterface == 0)
    {
         sensordLogD() << "Creating sensor client interface fails.";
         return false;
    }
    sensordLogD() << "Created sensor: " << m_sensorChannelInterface->description();
    sensordLogD() << "Support intervals: " << toString(m_sensorChannelInterface->getAvailableIntervals());
    sensordLogD() << "Support dataranges: " << toString(m_sensorChannelInterface->getAvailableDataRanges());
    m_sensorChannelInterface->setInterval(m_interval_ms);
    m_sensorChannelInterface->setBufferInterval(m_bufferinterval_ms);
    m_sensorChannelInterface->setBufferSize(m_buffersize);
    m_sensorChannelInterface->setStandbyOverride(m_standbyoverride);
    m_sensorChannelInterface->setDownsampling(m_downsample);
    m_sensorChannelInterface->start();

    return true;
}

bool SensorHandler::stopClient()
{
    if (m_sensorChannelInterface)
    {
        m_sensorChannelInterface->stop();
        delete m_sensorChannelInterface;
        m_sensorChannelInterface = 0;
    }
    return true;
}

bool SensorHandler::init(const QStringList& sensors)
{
    SensorManagerInterface& remoteSensorManager = SensorManagerInterface::instance();
    if(!remoteSensorManager.isValid())
    {
        sensordLogC() << "Failed to create SensorManagerInterface";
        return false;
    }
    foreach (const QString& sensorName, sensors)
    {
        QDBusReply<bool> reply(remoteSensorManager.loadPlugin(sensorName));
        if(!reply.isValid() || !reply.value())
        {
            sensordLogW() << "Failed to load plugin";
            return false;
        }

        if (sensorName == "orientationsensor"){
            remoteSensorManager.registerSensorInterface<OrientationSensorChannelInterface>(sensorName);
        } else if (sensorName == "accelerometersensor"){
            remoteSensorManager.registerSensorInterface<AccelerometerSensorChannelInterface>(sensorName);
        } else if (sensorName == "compasssensor"){
            remoteSensorManager.registerSensorInterface<CompassSensorChannelInterface>(sensorName);
        } else if (sensorName == "tapsensor"){
            remoteSensorManager.registerSensorInterface<TapSensorChannelInterface>(sensorName);
        } else if (sensorName == "alssensor"){
            remoteSensorManager.registerSensorInterface<ALSSensorChannelInterface>(sensorName);
        } else if (sensorName == "proximitysensor"){
            remoteSensorManager.registerSensorInterface<ProximitySensorChannelInterface>(sensorName);
        } else if (sensorName == "rotationsensor"){
            remoteSensorManager.registerSensorInterface<RotationSensorChannelInterface>(sensorName);
        } else if (sensorName == "magnetometersensor"){
            remoteSensorManager.registerSensorInterface<MagnetometerSensorChannelInterface>(sensorName);
        }
    }
    return true;
}

void SensorHandler::createSensorInterface()
{
    if (m_sensorName == "compasssensor")
    {
        m_sensorChannelInterface = CompassSensorChannelInterface::interface("compasssensor");
        connect(m_sensorChannelInterface, SIGNAL(dataAvailable(const Compass&)), this, SLOT(receivedData(const Compass&)));
    }
    else if (m_sensorName == "magnetometersensor")
    {
        m_sensorChannelInterface = MagnetometerSensorChannelInterface::interface("magnetometersensor");
        connect(m_sensorChannelInterface, SIGNAL(dataAvailable(const MagneticField&)), this, SLOT(receivedData(const MagneticField&)));
        connect(m_sensorChannelInterface, SIGNAL(frameAvailable(const QVector<MagneticField>&)), this, SLOT(receivedFrame(const QVector<MagneticField>&)));
    }
    else if (m_sensorName ==  "orientationsensor")
    {
        m_sensorChannelInterface = OrientationSensorChannelInterface::interface("orientationsensor");
        connect(m_sensorChannelInterface, SIGNAL(orientationChanged(const Unsigned&)), this, SLOT(receivedData(const Unsigned&)));
    }
    else if (m_sensorName == "accelerometersensor")
    {
        m_sensorChannelInterface = AccelerometerSensorChannelInterface::interface("accelerometersensor");
        connect(m_sensorChannelInterface, SIGNAL(dataAvailable(const XYZ&)), this, SLOT(receivedData(const XYZ&)));
        connect(m_sensorChannelInterface, SIGNAL(frameAvailable(const QVector<XYZ>&)), this, SLOT(receivedFrame(const QVector<XYZ>&)));
    }
    else if (m_sensorName == "alssensor")
    {
        m_sensorChannelInterface = ALSSensorChannelInterface::interface("alssensor");
        connect(m_sensorChannelInterface, SIGNAL(ALSChanged(const Unsigned&)), this, SLOT(receivedData(const Unsigned&)));
    }
    else if (m_sensorName == "rotationsensor")
    {
        m_sensorChannelInterface = RotationSensorChannelInterface::interface("rotationsensor");
        connect(m_sensorChannelInterface, SIGNAL(dataAvailable(const XYZ&)), this, SLOT(receivedData(const XYZ&)));
        connect(m_sensorChannelInterface, SIGNAL(frameAvailable(const QVector<XYZ>&)), this, SLOT(receivedFrame(const QVector<XYZ>&)));
    }
    else if (m_sensorName == "tapsensor")
    {
        m_sensorChannelInterface = TapSensorChannelInterface::interface("tapsensor");
        connect(m_sensorChannelInterface, SIGNAL(dataAvailable(const Tap&)), this, SLOT(receivedData(const Tap&)));
    }
    else if (m_sensorName == "proximitysensor")
    {
        m_sensorChannelInterface = ProximitySensorChannelInterface::interface("proximitysensor");
        connect(m_sensorChannelInterface, SIGNAL(reflectanceDataAvailable(const Proximity&)), this, SLOT(receivedData(const Proximity&)));
    }
}

QString SensorHandler::toString(const DataRangeList& ranges)
{
    QString str;
    bool first = true;
    foreach(const DataRange& range, ranges)
    {
        if(!first)
            str.append(", ");
        str.append("[")
            .append(QVariant(range.min).toString())
            .append(", ")
            .append(QVariant(range.max).toString())
            .append("]");
        first = false;
    }
    return str;
}
