/**
   @file stepcountersensor.cpp
   @brief stepcounter sensor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation
   Copyright (C) 2016 kimmoli

   @author Kimmo Lindholm <kimmo.lindholm@eke.fi>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   
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

#include "stepcountersensor.h"

#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"
#include "datatypes/orientation.h"

StepCounterSensorChannel::StepCounterSensorChannel(const QString& id) :
        AbstractSensorChannel(id),
        DataEmitter<TimedUnsigned>(1),
        previousValue_(0,0)
{
    SensorManager& sm = SensorManager::instance();

    stepcounterAdaptor_ = sm.requestDeviceAdaptor("stepcounteradaptor");
    if (!stepcounterAdaptor_) {
        setValid(false);
        return;
    }

    stepcounterReader_ = new BufferReader<TimedUnsigned>(1);

    outputBuffer_ = new RingBuffer<TimedUnsigned>(1);

    // Create buffers for filter chain
    filterBin_ = new Bin;

    filterBin_->add(stepcounterReader_, "stepcounter");
    filterBin_->add(outputBuffer_, "buffer");

    filterBin_->join("stepcounter", "source", "buffer", "sink");

    // Join datasources to the chain
    connectToSource(stepcounterAdaptor_, "stepcounter", stepcounterReader_);

    marshallingBin_ = new Bin;
    marshallingBin_->add(this, "sensorchannel");

    outputBuffer_->join(this);

    setDescription("steps since boot");
    setRangeSource(stepcounterAdaptor_);
    addStandbyOverrideSource(stepcounterAdaptor_);
    setIntervalSource(stepcounterAdaptor_);

    setValid(true);
}

StepCounterSensorChannel::~StepCounterSensorChannel()
{
    if (isValid()) {
        SensorManager& sm = SensorManager::instance();

        disconnectFromSource(stepcounterAdaptor_, "stepcounter", stepcounterReader_);

        sm.releaseDeviceAdaptor("stepcounteradaptor");

        delete stepcounterReader_;
        delete outputBuffer_;
        delete marshallingBin_;
        delete filterBin_;
    }
}

bool StepCounterSensorChannel::start()
{
    qCInfo(lcSensorFw) << id() << "Starting StepCounterSensorChannel";

    if (AbstractSensorChannel::start()) {
        marshallingBin_->start();
        filterBin_->start();
        stepcounterAdaptor_->startSensor();
    }
    return true;
}

bool StepCounterSensorChannel::stop()
{
    qCInfo(lcSensorFw) << id() << "Stopping StepCounterSensorChannel";

    if (AbstractSensorChannel::stop()) {
        stepcounterAdaptor_->stopSensor();
        filterBin_->stop();
        marshallingBin_->stop();
    }
    return true;
}

void StepCounterSensorChannel::emitData(const TimedUnsigned& value)
{
    if (value.value_ != previousValue_.value_) {
        previousValue_.value_ = value.value_;

        writeToClients((const void*)(&value), sizeof(value));
    }
}
