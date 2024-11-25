/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd

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

#include <QVariant>
#include <QStringList>
#include <QDebug>

#include "magcalibrationchain.h"
#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"
#include "config.h"
#include "logging.h"
#include "calibrationfilter.h"

#include "magcoordinatealignfilter.h"
#include "datatypes/orientationdata.h"
// magcalibrationchain requires: magnetometeradaptor, kbslideradaptor

MagCalibrationChain::MagCalibrationChain(const QString& id) :
    AbstractChain(id),
    filterBin(NULL),
    magAdaptor(NULL),
    magReader(NULL),
    magCalFilter(NULL),
    magScaleFilter(NULL),
    magCoordinateAlignFilter_(NULL),
    calibratedMagnetometerData(NULL)
{
    setMatrixFromString("1,0,0,\
                         0,1,0,\
                         0,0,1");
    SensorManager& sm = SensorManager::instance();

    magAdaptor = sm.requestDeviceAdaptor("magnetometeradaptor");
    // valid is false by default so no need to set it if magnetormeter adaptor is not there
    if (magAdaptor)
        setValid(magAdaptor->isValid());

// SensorFrameworkConfig::configuration()->value<int>("magnetometer/interval_compensation", 16);
    // Get the transformation matrix from config file
    QString aconvString = SensorFrameworkConfig::configuration()->value<QString>("magnetometer/transformation_matrix", "");
    if (aconvString.size() > 0) {
        if (!setMatrixFromString(aconvString)) {
            qCWarning(lcSensorFw)<< NodeBase::id() << "Failed to parse 'transformation_matrix' configuration key. Coordinate alignment may be invalid";
        }
    }

    needsCalibration = SensorFrameworkConfig::configuration()->value<bool>("magnetometer/needs_calibration", true);

    calibratedMagnetometerData = new RingBuffer<CalibratedMagneticFieldData>(1);
    nameOutputBuffer("calibratedmagnetometerdata", calibratedMagnetometerData);

    // Create buffers for filter chain
    filterBin = new Bin;
    //formationsink
    magReader = new BufferReader<CalibratedMagneticFieldData>(1);

    // Join filterchain buffers
    filterBin->add(magReader, "calibratedmagneticfield");

    filterBin->add(calibratedMagnetometerData, "calibratedmagnetometerdata"); //calibration

    if (sm.getAdaptorTypes().contains("orientationadaptor")) {
        DeviceAdaptor *orientAdaptor = sm.requestDeviceAdaptor("orientationadaptor");
        if (orientAdaptor && orientAdaptor->isValid()) {
            needsCalibration = false;
        }
    }
    magCoordinateAlignFilter_ = sm.instantiateFilter("magcoordinatealignfilter");
    Q_ASSERT(magCoordinateAlignFilter_);
    filterBin->add(magCoordinateAlignFilter_, "magcoordinatealigner");

    if (needsCalibration) {
        magCalFilter = sm.instantiateFilter("calibrationfilter");

        ((MagCoordinateAlignFilter*)magCoordinateAlignFilter_)->setMatrix(TMagMatrix(aconv_));

        filterBin->add(magCalFilter, "calibration");

        if (!filterBin->join("calibratedmagneticfield", "source", "magcoordinatealigner", "sink"))
            qDebug()<< NodeBase::id() << Q_FUNC_INFO << "calibratedmagneticfield/magcoordinatealigner join failed";

        if (!filterBin->join("magcoordinatealigner", "source", "calibration", "magsink"))
            qDebug()<< NodeBase::id() << Q_FUNC_INFO << "magcoordinatealigner/calibration join failed";

        if (!filterBin->join("calibration", "source", "calibratedmagnetometerdata", "sink"))
            qDebug()<< NodeBase::id() << Q_FUNC_INFO << "calibration/calibratedmagnetometerdata join failed";
    } else {
        if (!filterBin->join("calibratedmagneticfield", "source", "magcoordinatealigner", "sink"))
            qDebug()<< NodeBase::id() << Q_FUNC_INFO << "calibratedmagneticfield/magcoordinatealigner join failed";

        if (!filterBin->join("magcoordinatealigner", "source", "calibratedmagnetometerdata", "sink"))
            qDebug()<< NodeBase::id() << Q_FUNC_INFO << "magcoordinatealigner/calibratedmagnetometerdata join failed";
    }

    // Join datasources to the chain
    connectToSource(magAdaptor, "calibratedmagneticfield", magReader);

    setDescription("Calibrated Mag values"); //Magnetometer calibration
    setRangeSource(magAdaptor);
    addStandbyOverrideSource(magAdaptor);
    setIntervalSource(magAdaptor);
}

MagCalibrationChain::~MagCalibrationChain()
{
    SensorManager& sm = SensorManager::instance();
    sm.releaseDeviceAdaptor("magnetometeradaptor");
    disconnectFromSource(magAdaptor, "magnetometer", magReader);

    delete magReader;
    if (needsCalibration) {
        delete magCoordinateAlignFilter_;
        delete magCalFilter;
    }
    delete calibratedMagnetometerData;
    delete filterBin;
}

bool MagCalibrationChain::start()
{
    if (!magAdaptor) {
        qCInfo(lcSensorFw) << id() << "No magnetometer adaptor to start.";
        return false;
    }

    if (AbstractSensorChannel::start()) {
        qCInfo(lcSensorFw) << id() << "Starting MagCalibrationChain";
        filterBin->start();
        magAdaptor->startSensor();
    }
    return true;
}

bool MagCalibrationChain::stop()
{
    if (!magAdaptor) {
        qCInfo(lcSensorFw) << id() << "No magnetometer adaptor to stop.";
        return false;
    }

    if (AbstractSensorChannel::stop()) {
        qCInfo(lcSensorFw) << id() << "Stopping MagCalibrationChain";
        magAdaptor->stopSensor();
        filterBin->stop();
    }
    return true;
}

void MagCalibrationChain::resetCalibration()
{
   if (needsCalibration) {
       CalibrationFilter *filter = static_cast<CalibrationFilter *>(magCalFilter);
       if (!filter) {
           qCInfo(lcSensorFw) << id() << "Can not reset calibration without filter.";
           return;
       }
       filter->dropCalibration();
   }
}

bool MagCalibrationChain::setMatrixFromString(const QString& str)
{
    QStringList strList = str.split(',');
    if (strList.size() != 9) {
        qCWarning(lcSensorFw) << id() << "Invalid cell count from matrix. Expected 9, got" << strList.size();
        return false;
    }

    for (int i = 0; i < 9; ++i) {
        aconv_[i/3][i%3] = strList.at(i).toInt();
    }

    return true;
}

