/**
   @file magnetometeradaptor.cpp
   @brief MagnetometerAdaptor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
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

#include "logging.h"
#include "config.h"
#include "magnetometeradaptor.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include "datatypes/utils.h"
#include <unistd.h>
#include <QFile>

/* Device name: /dev/ak8974n, where n is a running number (0 in case on single chip configuration) */
struct ak8974_data {
        __s16 x; /* 0.3uT */
        __s16 y; /* 0.3uT */
        __s16 z; /* 0.3uT */
        __u16 valid;
}; //__attribute__((packed)); <-- documentation states that this is a nogo for c++

MagnetometerAdaptor::MagnetometerAdaptor(const QString& id) :
    SysfsAdaptor(id, SysfsAdaptor::IntervalMode, false)
{
    int intervalCompensation_ms = SensorFrameworkConfig::configuration()->value<int>("magnetometer/interval_compensation", 0);
    m_intervalCompensation_us = intervalCompensation_ms * 1000;
    m_magnetometerBuffer = new DeviceAdaptorRingBuffer<CalibratedMagneticFieldData>(1);
    setAdaptedSensor("magnetometer", "Internal magnetometer coordinates", m_magnetometerBuffer);
    m_overflowLimit = SensorFrameworkConfig::configuration()->value<int>("magnetometer/overflow_limit", 8000);
    setDescription("Input device Magnetometer adaptor (ak897x)");
}

MagnetometerAdaptor::~MagnetometerAdaptor()
{
    delete m_magnetometerBuffer;
}

void MagnetometerAdaptor::processSample(int pathId, int fd)
{
    Q_UNUSED(pathId);

    struct ak8974_data mag_data;

    unsigned int bytesRead = read(fd, &mag_data, sizeof(mag_data));

    if (bytesRead < sizeof(mag_data)) {
        sensordLogW() << id() << "read " << bytesRead  << " bytes out of expected " << sizeof(mag_data) << " bytes. Previous error: " << strerror(errno);
        //return;
    }

    if (!mag_data.valid) {
        // Can't trust this, printed for curiosity
        sensordLogD() << id() << "Invalid sample received from magnetometer";
    }

    sensordLogT() << id() << "Magnetometer reading: " << mag_data.x << ", " << mag_data.y << ", " << mag_data.z;

    CalibratedMagneticFieldData *sample = m_magnetometerBuffer->nextSlot();

    sample->timestamp_ = Utils::getTimeStamp();
    sample->x_ = mag_data.x;
    sample->y_ = mag_data.y;
    sample->z_ = mag_data.z;

    m_magnetometerBuffer->commit();
    m_magnetometerBuffer->wakeUpReaders();
}

bool MagnetometerAdaptor::setInterval(const int sessionId, const unsigned int interval_us)
{
    if(m_intervalCompensation_us)
    {
        return SysfsAdaptor::setInterval(sessionId, (signed)interval_us >m_intervalCompensation_us ? interval_us - m_intervalCompensation_us : 0);
    }
    return SysfsAdaptor::setInterval(sessionId, interval_us);
}

void MagnetometerAdaptor::setOverflowLimit(int limit)
{
    m_overflowLimit = limit;
}

int MagnetometerAdaptor::overflowLimit() const
{
    return m_overflowLimit;
}
