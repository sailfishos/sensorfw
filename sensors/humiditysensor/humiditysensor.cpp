/**
   @file humiditysensor.cpp
   @brief humiditySensor

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

#include "humiditysensor.h"

#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"
#include "datatypes/orientation.h"

HumiditySensorChannel::HumiditySensorChannel(const QString& id) :
        AbstractSensorChannel(id),
        DataEmitter<TimedUnsigned>(1),
        previousRelativeValue_(0,0)
{
    SensorManager& sm = SensorManager::instance();

    humidityAdaptor_ = sm.requestDeviceAdaptor("humidityadaptor");
    if (!humidityAdaptor_) {
        setValid(false);
        return;
    }

    humidityReader_ = new BufferReader<TimedUnsigned>(1);

    outputBuffer_ = new RingBuffer<TimedUnsigned>(1);

    // Create buffers for filter chain
    filterBin_ = new Bin;

    filterBin_->add(humidityReader_, "humidity");
    filterBin_->add(outputBuffer_, "buffer");

    filterBin_->join("humidity", "source", "buffer", "sink");

    // Join datasources to the chain
    connectToSource(humidityAdaptor_, "humidity", humidityReader_);

    marshallingBin_ = new Bin;
    marshallingBin_->add(this, "sensorchannel");

    outputBuffer_->join(this);


    setDescription("relative humidity in percentage");
    setRangeSource(humidityAdaptor_);
    addStandbyOverrideSource(humidityAdaptor_);
    setIntervalSource(humidityAdaptor_);

    setValid(true);
}

HumiditySensorChannel::~HumiditySensorChannel()
{
    if (isValid()) {
        SensorManager& sm = SensorManager::instance();

        disconnectFromSource(humidityAdaptor_, "humidity", humidityReader_);

        sm.releaseDeviceAdaptor("humidityadaptor");

        delete humidityReader_;
        delete outputBuffer_;
        delete marshallingBin_;
        delete filterBin_;
    }
}

bool HumiditySensorChannel::start()
{
    qCInfo(lcSensorFw) << id() << "Starting HumiditySensorChannel";

    if (AbstractSensorChannel::start()) {
        marshallingBin_->start();
        filterBin_->start();
        humidityAdaptor_->startSensor();
    }
    return true;
}

bool HumiditySensorChannel::stop()
{
    qCInfo(lcSensorFw) << id() << "Stopping HumiditySensorChannel";

    if (AbstractSensorChannel::stop()) {
        humidityAdaptor_->stopSensor();
        filterBin_->stop();
        marshallingBin_->stop();
    }
    return true;
}

void HumiditySensorChannel::emitData(const TimedUnsigned& value)
{
    if (value.value_ != previousRelativeValue_.value_) {
        previousRelativeValue_.value_ = value.value_;

        writeToClients((const void*)(&value), sizeof(value));
    }
}
