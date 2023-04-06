/**
   @file temperaturesensor.cpp
   @brief TemperatureSensor

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

#include "temperaturesensor.h"

#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"
#include "datatypes/orientation.h"

TemperatureSensorChannel::TemperatureSensorChannel(const QString& id) :
        AbstractSensorChannel(id),
        DataEmitter<TimedUnsigned>(1),
        previousValue_(0,0)
{
    SensorManager& sm = SensorManager::instance();

    temperatureAdaptor_ = sm.requestDeviceAdaptor("temperatureadaptor");
    if (!temperatureAdaptor_) {
        setValid(false);
        return;
    }

    temperatureReader_ = new BufferReader<TimedUnsigned>(1);

    outputBuffer_ = new RingBuffer<TimedUnsigned>(1);

    // Create buffers for filter chain
    filterBin_ = new Bin;

    filterBin_->add(temperatureReader_, "temperature");
    filterBin_->add(outputBuffer_, "buffer");

    filterBin_->join("temperature", "source", "buffer", "sink");

    // Join datasources to the chain
    connectToSource(temperatureAdaptor_, "temperature", temperatureReader_);

    marshallingBin_ = new Bin;
    marshallingBin_->add(this, "sensorchannel");

    outputBuffer_->join(this);

    setDescription("ambient temperature in celsius");
    setRangeSource(temperatureAdaptor_);
    addStandbyOverrideSource(temperatureAdaptor_);
    setIntervalSource(temperatureAdaptor_);

    setValid(true);
}

TemperatureSensorChannel::~TemperatureSensorChannel()
{
    if (isValid()) {
        SensorManager& sm = SensorManager::instance();

        disconnectFromSource(temperatureAdaptor_, "temperature", temperatureReader_);

        sm.releaseDeviceAdaptor("temperatureadaptor");

        delete temperatureReader_;
        delete outputBuffer_;
        delete marshallingBin_;
        delete filterBin_;
    }
}

bool TemperatureSensorChannel::start()
{
    sensordLogD() << id() << "Starting TemperatureSensorChannel";

    if (AbstractSensorChannel::start()) {
        marshallingBin_->start();
        filterBin_->start();
        temperatureAdaptor_->startSensor();
    }
    return true;
}

bool TemperatureSensorChannel::stop()
{
    sensordLogD() << id() << "Stopping TemperatureSensorChannel";

    if (AbstractSensorChannel::stop()) {
        temperatureAdaptor_->stopSensor();
        filterBin_->stop();
        marshallingBin_->stop();
    }
    return true;
}

void TemperatureSensorChannel::emitData(const TimedUnsigned& value)
{
    if (value.value_ != previousValue_.value_) {
        previousValue_.value_ = value.value_;

        writeToClients((const void*)(&value), sizeof(value));
    }
}
