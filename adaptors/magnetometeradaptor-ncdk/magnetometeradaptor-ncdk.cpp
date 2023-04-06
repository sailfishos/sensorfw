/**
   @file magnetometeradaptor-ncdk.cpp
   @brief MagnetometerAdaptor for ncdk

   <p>
   Copyright (C) 2010-2011 Nokia Corporation

   @author Shenghua Liu <ext-shenghua.1.liu@nokia.com>

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
#include "magnetometeradaptor-ncdk.h"
#include <QString>
#include "config.h"
#include <errno.h>
#include "datatypes/utils.h"
#include "logging.h"
#include <unistd.h>

MagnetometerAdaptorNCDK::MagnetometerAdaptorNCDK(const QString& id) :
    SysfsAdaptor(id, SysfsAdaptor::IntervalMode),
    m_powerState(false)
{
    int intervalCompensation_ms = SensorFrameworkConfig::configuration()->value<int>("magnetometer/interval_compensation", 0);
    m_intervalCompensation_us = intervalCompensation_ms * 1000;
    m_powerStateFilePath = SensorFrameworkConfig::configuration()->value<QByteArray>("magnetometer/path_power_state", "");
    m_sensAdjFilePath = SensorFrameworkConfig::configuration()->value<QByteArray>("magnetometer/path_sens_adjust", "");
    m_magnetometerBuffer = new DeviceAdaptorRingBuffer<CalibratedMagneticFieldData>(128);
    setAdaptedSensor("magnetometer", "Internal magnetometer coordinates", m_magnetometerBuffer);
    setDescription("Magnetometer adaptor (ak8975) for NCDK");

    //get sensitivity adjustment
    getSensitivityAdjustment(m_x_adj, m_y_adj, m_z_adj);

    m_overflowLimit = SensorFrameworkConfig::configuration()->value<int>("magnetometer/overflow_limit", 8000);
}

MagnetometerAdaptorNCDK::~MagnetometerAdaptorNCDK()
{
    delete m_magnetometerBuffer;
}

void MagnetometerAdaptorNCDK::processSample(int pathId, int fd)
{
    Q_UNUSED(pathId);

    if (!m_powerState)
        return;

    char buf[32];
    int x = 0, y = 0, z = 0;

    QList<QByteArray> strList;
    int bytesRead = 0;
    bool isOK = (bytesRead = read(fd, &buf, sizeof(buf))) > 0;

    if (isOK) {
        strList = QByteArray(buf, bytesRead).split(':');
        if (strList.size() == 3) {
            x = adjustPos(strList.at(0).toInt(), m_x_adj);
            y = adjustPos(strList.at(1).toInt(), m_y_adj);
            z = adjustPos(strList.at(2).toInt(), m_z_adj);
        }
    } else {
        sensordLogW() << id() << "Reading magnetometer error: " << strerror(errno);
        return;
    }

    sensordLogT() << id() << "Magnetometer Reading: " << x << ", " << y << ", " << z;

    CalibratedMagneticFieldData *sample = m_magnetometerBuffer->nextSlot();

    sample->timestamp_ = Utils::getTimeStamp();
    sample->x_ = x;
    sample->y_ = y;
    sample->z_ = z;

    m_magnetometerBuffer->commit();
    m_magnetometerBuffer->wakeUpReaders();
}

bool MagnetometerAdaptorNCDK::setPowerState(bool value) const
{
    sensordLogD() << id() << "Setting power state for compass driver" << " to " << value;

    QByteArray powerStateStr = QByteArray::number(value);

    if (!writeToFile(m_powerStateFilePath, powerStateStr))
    {
        sensordLogW() << id() << "Unable to set power state for compass driver";
        return false;
    }
    return true;
}

void MagnetometerAdaptorNCDK::getSensitivityAdjustment(int &x, int &y, int &z) const
{
    QByteArray byteArray = readFromFile(m_sensAdjFilePath);

    QList<QByteArray> strList = byteArray.split(':');
    if (strList.size() == 3)
    {
        x = strList.at(0).toInt();
        y = strList.at(1).toInt();
        z = strList.at(2).toInt();
    }
}

int MagnetometerAdaptorNCDK::adjustPos(const int value, const int adj) const
{
    return value * ( adj + 128 ) / 256;
}

bool MagnetometerAdaptorNCDK::startSensor()
{
    if (!setPowerState(true))
    {
        sensordLogW() << id() << "Unable to set power on for compass driver";
    }
    else
    {
        m_powerState = true;
    }

    return SysfsAdaptor::startSensor();
}

void MagnetometerAdaptorNCDK::stopSensor()
{
    if (!setPowerState(false))
    {
        sensordLogW() << id() << "Unable to set power off for compass driver";
    }
    else
    {
        m_powerState = false;
    }

    SysfsAdaptor::stopSensor();
}

bool MagnetometerAdaptorNCDK::setInterval(const int sessionId, const unsigned int interval_us)
{
    if(m_intervalCompensation_us)
    {
        return SysfsAdaptor::setInterval(sessionId, (int)interval_us > m_intervalCompensation_us ? interval_us - m_intervalCompensation_us : 0);
    }
    return SysfsAdaptor::setInterval(sessionId, interval_us);
}

void MagnetometerAdaptorNCDK::setOverflowLimit(int limit)
{
    m_overflowLimit = limit;
}

int MagnetometerAdaptorNCDK::overflowLimit() const
{
    return m_overflowLimit;
}
