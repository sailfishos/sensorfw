/**
   @file orientationchain.cpp
   @brief OrientationChain

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>

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

#include "orientationchain.h"
#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"

OrientationChain::OrientationChain(const QString& id) :
    AbstractChain(id)
{
    SensorManager& sm = SensorManager::instance();

    accelerometerChain_ = sm.requestChain("accelerometerchain");
    Q_ASSERT( accelerometerChain_ );
    setValid(accelerometerChain_->isValid());

    accelerometerReader_ = new BufferReader<AccelerationData>(1);

    orientationInterpreterFilter_ = sm.instantiateFilter("orientationinterpreter");

    topEdgeOutput_ = new RingBuffer<PoseData>(1);
    nameOutputBuffer("topedge", topEdgeOutput_);

    faceOutput_ = new RingBuffer<PoseData>(1);
    nameOutputBuffer("face", faceOutput_);

    orientationOutput_ = new RingBuffer<PoseData>(1);
    nameOutputBuffer("orientation", orientationOutput_);

    // Create buffers for filter chain
    filterBin_ = new Bin;

    filterBin_->add(accelerometerReader_, "accelerometer");
    filterBin_->add(orientationInterpreterFilter_, "orientationinterpreter");
    filterBin_->add(topEdgeOutput_, "topedgebuffer");
    filterBin_->add(faceOutput_, "facebuffer");
    filterBin_->add(orientationOutput_, "orientationbuffer");

    // Join filterchain buffers
    if (!filterBin_->join("accelerometer", "source", "orientationinterpreter", "accsink"))
        qDebug()<< NodeBase::id() << Q_FUNC_INFO << "accelerometer/orientationinterpreter join failed";
    if (!filterBin_->join("orientationinterpreter", "topedge", "topedgebuffer", "sink"))
        qDebug()<< NodeBase::id() << Q_FUNC_INFO << "orientationinterpreter/topedgebuffer join failed";
    if (!filterBin_->join("orientationinterpreter", "face", "facebuffer", "sink"))
        qDebug()<< NodeBase::id() << Q_FUNC_INFO << "orientationinterpreter/facebuffer join failed";
    if (!filterBin_->join("orientationinterpreter", "orientation", "orientationbuffer", "sink"))
        qDebug()<< NodeBase::id() << Q_FUNC_INFO << "orientationinterpreter/orientationbuffer join failed";

    // Join datasources to the chain
    connectToSource(accelerometerChain_, "accelerometer", accelerometerReader_);

    setDescription("Device orientation interpretations (in different flavors)");
    introduceAvailableDataRange(DataRange(0, 6, 1));
    addStandbyOverrideSource(accelerometerChain_);
    setIntervalSource(accelerometerChain_);
}

OrientationChain::~OrientationChain()
{
    disconnectFromSource(accelerometerChain_, "accelerometer", accelerometerReader_);

    delete accelerometerReader_;
    delete orientationInterpreterFilter_;
    delete topEdgeOutput_;
    delete faceOutput_;
    delete orientationOutput_;
    delete filterBin_;
}

bool OrientationChain::start()
{
    if (AbstractSensorChannel::start()) {
        qCInfo(lcSensorFw) << id() << "Starting AccelerometerChain";
        filterBin_->start();
        accelerometerChain_->start();
    }
    return true;
}

bool OrientationChain::stop()
{
    if (AbstractSensorChannel::stop()) {
        qCInfo(lcSensorFw) << id() << "Stopping AccelerometerChain";
        accelerometerChain_->stop();
        filterBin_->stop();
    }
    return true;
}
