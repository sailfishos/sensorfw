/**
   @file proximityadaptor.cpp
   @brief ProximityAdaptor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Matias Muhonen <ext-matias.muhonen@nokia.com>
   @author Lihan Guo <ext-lihan.4.guo@nokia.com>
   @author Shenghua <ext-shenghua.1.liu@nokia.com>
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
#include "proximityadaptor.h"
#include <errno.h>
#include "datatypes/utils.h"
#include "logging.h"
#include "config.h"
#include <QFile>
#include <linux/types.h>
#include <unistd.h>

struct bh1770glc_ps {
    __u8 led1;
    __u8 led2;
    __u8 led3;
} __attribute__((packed));

struct apds990x_data {
        __u32 lux; /* 10x scale */
        __u32 lux_raw; /* 10x scale */
        __u16 ps;
        __u16 ps_raw;
        __u16 status;
} __attribute__((packed));

ProximityAdaptor::ProximityAdaptor(const QString& id) :
    SysfsAdaptor(id, SysfsAdaptor::SelectMode, false)
{
    deviceType_ = (DeviceType)SensorFrameworkConfig::configuration()->value<int>("proximity/driver_type", 0);
    threshold_ = SensorFrameworkConfig::configuration()->value<int>("proximity/threshold", 35);
    powerStatePath_ = SensorFrameworkConfig::configuration()->value("proximity/powerstate_path").toByteArray();
    proximityBuffer_ = new DeviceAdaptorRingBuffer<ProximityData>(1);
    setAdaptedSensor("proximity", "Proximity state", proximityBuffer_);
    setDescription("Proximity sensor readings (Dipro sensor)");
}

ProximityAdaptor::~ProximityAdaptor()
{
    stopSensor();
    delete proximityBuffer_;
}

bool ProximityAdaptor::startSensor()
{
    if (!powerStatePath_.isEmpty()) {
        writeToFile(powerStatePath_, "1");
    }
    return SysfsAdaptor::startSensor();
}

void ProximityAdaptor::stopSensor()
{
    if (!powerStatePath_.isEmpty()) {
        writeToFile(powerStatePath_, "0");
    }
    SysfsAdaptor::stopSensor();
}

void ProximityAdaptor::processSample(int pathId, int fd)
{
    Q_UNUSED(pathId);

    int ret = 0;
    unsigned rawdata = 0;

    if (deviceType_ == RM680) {
        bh1770glc_ps ps_data;
        int bytesRead = read(fd, &ps_data, sizeof(ps_data));

        if (bytesRead > 0) {
            sensordLogT() << id() << "Proximity Values: " << ps_data.led1 << ", " << ps_data.led2 << ", " <<  ps_data.led3;
        } else {
            sensordLogW() << id() << "read(): " << strerror(errno);
            return;
        }

        if ( ps_data.led1 > threshold_ ) {
            ret = 1;
        }
        rawdata = ps_data.led1;
    } else if (deviceType_ == RM696) {
        apds990x_data ps_data;
        int bytesRead = read(fd, &ps_data, sizeof(ps_data));

        if (bytesRead > 0) {
            sensordLogT() << id() << "Proximity Values: " << ps_data.ps << ", " << ps_data.ps_raw << ", " << ps_data.status;
        } else {
            sensordLogW() << id() << "read(): " << strerror(errno);
            return;
        }

        if (ps_data.ps > threshold_) {
            ret = 1;
        }
        rawdata = ps_data.ps;
    } else if (deviceType_ == NCDK) {
        char buffer[100];
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = read(fd, &buffer, sizeof(buffer));
        if (bytesRead <= 0) {
            sensordLogW() << id() << "read(): " << strerror(errno);
            return;
        }
        sscanf(buffer, "%d", &rawdata);
        if ((signed)rawdata > threshold_) {
            ret = 1;
        }
        sensordLogT() << id() << "Proximity value: " << rawdata;
    } else {
        sensordLogW() << id() << "Not known device type: " << deviceType_;
        return;
    }

    ProximityData* proximityData = proximityBuffer_->nextSlot();

    proximityData->timestamp_ = Utils::getTimeStamp();
    proximityData->withinProximity_ = ret;
    proximityData->value_ = rawdata;
    proximityBuffer_->commit();
    proximityBuffer_->wakeUpReaders();
}
