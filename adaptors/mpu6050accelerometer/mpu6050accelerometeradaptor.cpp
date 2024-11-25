/**
   Copyright 2013 Ruediger Gad <r.c.g@gmx.de>

   The Mpu6050AccelAdaptor was created based on the OemTabletAccelAdaptor.

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
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

#include "config.h"
#include "mpu6050accelerometeradaptor.h"
#include "logging.h"
#include "datatypes/utils.h"

#define EARTH_GRAVITY 9.81
/*
 * 165.8 is the result of a reference measurement.
 * We are going to correct it such that we get the correct acceleration in m/s^2
 */
#define CORRECTION_FACTOR float(165.8 / EARTH_GRAVITY)

Mpu6050AccelAdaptor::Mpu6050AccelAdaptor (const QString& id) :
    SysfsAdaptor (id, SysfsAdaptor::IntervalMode)
{
    struct stat st;

    QString xAxisPath = SensorFrameworkConfig::configuration()->value("accelerometer/x_axis_path").toString ();
    if ( lstat (xAxisPath.toLatin1().constData(), &st) < 0 ) {
        qCWarning(lcSensorFw) << "x_axis_path: " << xAxisPath << " not found";
        return;
    }
    addPath(xAxisPath, X_AXIS);

    QString yAxisPath = SensorFrameworkConfig::configuration()->value("accelerometer/y_axis_path").toString ();
    if ( lstat (yAxisPath.toLatin1().constData(), &st) < 0 ) {
        qCWarning(lcSensorFw) << "y_axis_path: " << yAxisPath << " not found";
        return;
    }
    addPath(yAxisPath, Y_AXIS);

    QString zAxisPath = SensorFrameworkConfig::configuration()->value("accelerometer/z_axis_path").toString ();
    if ( lstat (zAxisPath.toLatin1().constData(), &st) < 0 ) {
        qCWarning(lcSensorFw) << "z_axis_path: " << zAxisPath << " not found";
        return;
    }
    addPath(zAxisPath, Z_AXIS);

    buffer = new DeviceAdaptorRingBuffer<OrientationData>(128);
    setAdaptedSensor("accelerometer", "MPU6050 accelerometer", buffer);

    setDescription("MPU 6050 accelerometer");
//    introduceAvailableDataRange(DataRange(-16384, 16384, 1));
//    unsigned int min_interval_us =  10 * 1000;
//    unsigned int max_interval_us = 586 * 1000;
//    introduceAvailableInterval(DataRange(min_interval_us, max_interval_us, 0));
//    unsigned int interval_us = 100 * 1000;
//    setDefaultInterval(interval_us);
}

Mpu6050AccelAdaptor::~Mpu6050AccelAdaptor () {
    delete buffer;
}

bool Mpu6050AccelAdaptor::startSensor () {
    if ( !(SysfsAdaptor::startSensor ()) )
        return false;

    qCInfo(lcSensorFw) << id() << "MPU6050 AccelAdaptor start";
    return true;
}

void Mpu6050AccelAdaptor::stopSensor () {
    SysfsAdaptor::stopSensor();
    qCInfo(lcSensorFw) << id() << "MPU6050 AccelAdaptor stop";
}

void Mpu6050AccelAdaptor::processSample (int pathId, int fd) {
    char buf[32];
    int val;

    if ( pathId < X_AXIS || pathId > Z_AXIS ) {
        qCWarning(lcSensorFw) << id() << "Wrong pathId: " << pathId;
        return;
    }

    lseek (fd, 0, SEEK_SET);
    if (read (fd, buf, sizeof(buf)) < 0) {
        qCWarning(lcSensorFw) << id() << "Read failed";
        return;
    }

    if (sscanf (buf, "%d", &val) == 0 ) {
        qCWarning(lcSensorFw) << id() << "Wrong data format: " << buf;
        return;
    }

    switch (pathId) {
        case X_AXIS:
            currentData = buffer->nextSlot();                
            currentData->timestamp_ = Utils::getTimeStamp();
            currentData->x_ = val / CORRECTION_FACTOR;
            break;
        case Y_AXIS:
            currentData->y_ = val / CORRECTION_FACTOR;
            break;
        case Z_AXIS:
            currentData->z_ = val / CORRECTION_FACTOR;
            buffer->commit();
            buffer->wakeUpReaders();
            break;
        default:
            qCWarning(lcSensorFw) << id() << "Invalid pathId: " << pathId;
            break;
    }
}

