/**
   @file lidsensor.cpp
   @brief LidSensor

   <p>
   Copyright (C) 2016 Canonical,  Ltd.

   @author Lorn Potter <lorn.potter@canonical.com>

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

#include "lidsensor.h"

#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"
#include "datatypes/orientation.h"
#include "datatypes/lid.h"

LidSensorChannel::LidSensorChannel(const QString& id) :
        AbstractSensorChannel(id),
        DataEmitter<LidData>(1),
        previousValue_(0, LidData::FrontLid, 0)
{
    SensorManager& sm = SensorManager::instance();

    lidAdaptor_ = sm.requestDeviceAdaptor("lidsensoradaptor");
    if (!lidAdaptor_) {
        setValid(false);
        return;
    }

    lidReader_ = new BufferReader<LidData>(1);

    outputBuffer_ = new RingBuffer<LidData>(1);

    // Create buffers for filter chain
    filterBin_ = new Bin;

    filterBin_->add(lidReader_, "lid");
    filterBin_->add(outputBuffer_, "buffer");

    filterBin_->join("lid", "source", "buffer", "sink");

    // Join datasources to the chain
    connectToSource(lidAdaptor_, "lid", lidReader_);

    marshallingBin_ = new Bin;
    marshallingBin_->add(this, "sensorchannel");

    outputBuffer_->join(this);

    setDescription("lid closed");
    setRangeSource(lidAdaptor_);
    addStandbyOverrideSource(lidAdaptor_);
    setIntervalSource(lidAdaptor_);

    setValid(true);
}

LidSensorChannel::~LidSensorChannel()
{
    if (isValid()) {
        SensorManager& sm = SensorManager::instance();

        disconnectFromSource(lidAdaptor_, "lid", lidReader_);

        sm.releaseDeviceAdaptor("lidadaptor");

        delete lidReader_;
        delete outputBuffer_;
        delete marshallingBin_;
        delete filterBin_;
    }
}

bool LidSensorChannel::start()
{
    qCInfo(lcSensorFw) << id() << "Starting LidSensorChannel";

    if (AbstractSensorChannel::start()) {
        marshallingBin_->start();
        filterBin_->start();
        lidAdaptor_->startSensor();
    }
    return true;
}

bool LidSensorChannel::stop()
{
    qCInfo(lcSensorFw) << id() << "Stopping LidSensorChannel";

    if (AbstractSensorChannel::stop()) {
        lidAdaptor_->stopSensor();
        filterBin_->stop();
        marshallingBin_->stop();
    }
    return true;
}

void LidSensorChannel::emitData(const LidData& value)
{
    if (value.value_ != previousValue_.value_) {
        previousValue_.value_ = value.value_;

        writeToClients((const void*)(&value), sizeof(value));
    }
}
