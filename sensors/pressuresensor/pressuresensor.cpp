/**
   @file pressuresensor.cpp
   @brief PressureSensor

   <p>
   Copyright (C) 2016 Canonical LTD.

   @author Lorn Potter <lorn.potter@canonical.com>

   This file is part of Sensorfw.

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

#include "pressuresensor.h"

#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"

PressureSensorChannel::PressureSensorChannel(const QString& id) :
        AbstractSensorChannel(id),
        DataEmitter<TimedUnsigned>(1),
        previousValue_(0,0)
{
    SensorManager& sm = SensorManager::instance();

    pressureAdaptor_ = sm.requestDeviceAdaptor("pressureadaptor");
    if (!pressureAdaptor_) {
        setValid(false);
        return;
    }

    pressureReader_ = new BufferReader<TimedUnsigned>(1);

    outputBuffer_ = new RingBuffer<TimedUnsigned>(1);

    // Create buffers for filter chain
    filterBin_ = new Bin;

    filterBin_->add(pressureReader_, "pressure");
    filterBin_->add(outputBuffer_, "buffer");

    filterBin_->join("pressure", "source", "buffer", "sink");

    // Join datasources to the chain
    connectToSource(pressureAdaptor_, "pressure", pressureReader_);

    marshallingBin_ = new Bin;
    marshallingBin_->add(this, "sensorchannel");

    outputBuffer_->join(this);

    setDescription("ambient pressure in pascals");
    setRangeSource(pressureAdaptor_);
    addStandbyOverrideSource(pressureAdaptor_);
    setIntervalSource(pressureAdaptor_);

    setValid(true);
}

PressureSensorChannel::~PressureSensorChannel()
{
    if (isValid()) {
        SensorManager& sm = SensorManager::instance();

        disconnectFromSource(pressureAdaptor_, "pressure", pressureReader_);

        sm.releaseDeviceAdaptor("pressureadaptor");

        delete pressureReader_;
        delete outputBuffer_;
        delete marshallingBin_;
        delete filterBin_;
    }
}

bool PressureSensorChannel::start()
{
    sensordLogD() << id() << "Starting PressureSensorChannel";

    if (AbstractSensorChannel::start()) {
        marshallingBin_->start();
        filterBin_->start();
        pressureAdaptor_->startSensor();
    }
    return true;
}

bool PressureSensorChannel::stop()
{
    sensordLogD() << id() << "Stopping PressureSensorChannel";

    if (AbstractSensorChannel::stop()) {
        pressureAdaptor_->stopSensor();
        filterBin_->stop();
        marshallingBin_->stop();
    }
    return true;
}

void PressureSensorChannel::emitData(const TimedUnsigned& value)
{
    if (value.value_ != previousValue_.value_) {
        previousValue_.value_ = value.value_;

        writeToClients((const void*)(&value), sizeof(value));
    }
}
