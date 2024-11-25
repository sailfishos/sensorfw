/**
   @file magnetometeradaptor-ascii.cpp
   @brief MagnetometerAdaptor that reads magnetic value in X-axis, Y-axis, and Z-axis from ascii interface

   <p>
   Copyright (C) 2009-2010 Intel Corporation
   Copyright (C) 2009-2010 Nokia Corporation

   @author Leo Yan <leo.yan@intel.com>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Matias Muhonen <ext-matias.muhonen@nokia.com>
   @author Tapio Rantala <ext-tapio.rantala@nokia.com>
   @author Antti Virtanen <antti.i.virtanen@nokia.com>
   @author Lihan Guo <ext-lihan.4.guo@nokia.com>
   @author Shenghua <ext-shenghua.1.liu@nokia.com>

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

#include <errno.h>
#include <string.h>
#include "logging.h"
#include "config.h"
#include "magnetometeradaptor-ascii.h"
#include "datatypes/utils.h"
#include <unistd.h>

MagnetometerAdaptorAscii::MagnetometerAdaptorAscii(const QString& id) :
    SysfsAdaptor(id, SysfsAdaptor::IntervalMode)
{
    memset(buf, 0x0, 32);
    magnetBuffer_ = new DeviceAdaptorRingBuffer<CalibratedMagneticFieldData>(1);
    setAdaptedSensor("magnetometer", "ak8974 ascii", magnetBuffer_);
}

MagnetometerAdaptorAscii::~MagnetometerAdaptorAscii()
{
    delete magnetBuffer_;
}

void MagnetometerAdaptorAscii::processSample(int, int fd)
{
    unsigned short x, y, z;

    lseek(fd, 0, SEEK_SET);
    if (read(fd, buf, sizeof(buf)) <= 0) {
        qCWarning(lcSensorFw) << id() << "read(): " << strerror(errno);
        return;
    }
    qCDebug(lcSensorFw) << id() << "Magnetometer output value: " << buf;

    sscanf(buf, "%hx:%hx:%hx\n", &x, &y, &z);

    CalibratedMagneticFieldData* pos = magnetBuffer_->nextSlot();
    pos->x_ = (short)x;
    pos->y_ = (short)y;
    pos->z_ = (short)z;
    pos->timestamp_ = Utils::getTimeStamp();

    magnetBuffer_->commit();
    magnetBuffer_->wakeUpReaders();
}
