/**
   @file compasssensor.cpp
   @brief CompassSensor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
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

#include "compasssensor.h"
#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"
#include "logging.h"

CompassSensorChannel::CompassSensorChannel(const QString& id) :
        AbstractSensorChannel(id),
        DataEmitter<CompassData>(1),
        compassData(0, -1, -1)
{
    SensorManager& sm = SensorManager::instance();

    compassChain_ = sm.requestChain("compasschain");
    if (!compassChain_) {
        setValid(false);
        return;
    }
    setValid(compassChain_->isValid());

    inputReader_ = new BufferReader<CompassData>(1);

    outputBuffer_ = new RingBuffer<CompassData>(1);

    // Create buffers for filter chain
    filterBin_ = new Bin;

    filterBin_->add(inputReader_, "input");
    filterBin_->add(outputBuffer_, "output");

    // Join filterchain buffers
    filterBin_->join("input", "source", "output", "sink");

    connectToSource(compassChain_, "truenorth", inputReader_);

    marshallingBin_ = new Bin;
    marshallingBin_->add(this, "sensorchannel");

    outputBuffer_->join(this);

    setDescription("compass north in degrees");
    addStandbyOverrideSource(compassChain_);
    setIntervalSource(compassChain_);
    setRangeSource(compassChain_);
}

CompassSensorChannel::~CompassSensorChannel()
{
    if (isValid()) {
        SensorManager& sm = SensorManager::instance();

        disconnectFromSource(compassChain_, "truenorth", inputReader_);
        sm.releaseChain("compasschain");

        delete inputReader_;
        delete outputBuffer_;
        delete marshallingBin_;
        delete filterBin_;
    }
}

quint16 CompassSensorChannel::declinationValue() const
{
    return qvariant_cast<quint16>(compassChain_->property("declinationvalue"));
}

bool CompassSensorChannel::start()
{
    qCInfo(lcSensorFw) << id() << "Starting CompassSensorChannel";

    if (AbstractSensorChannel::start()) {
        marshallingBin_->start();
        filterBin_->start();
        compassChain_->setProperty("compassEnabled", true);
        compassChain_->start();
    }
    return true;
}

bool CompassSensorChannel::stop()
{
    qCInfo(lcSensorFw) << id() << "Stopping CompassSensorChannel";

    if (AbstractSensorChannel::stop()) {
        compassChain_->stop();
        compassChain_->setProperty("compassEnabled", false);
        filterBin_->stop();
        marshallingBin_->stop();
    }
    return true;
}

void CompassSensorChannel::emitData(const CompassData& value)
{
    compassData = value;
    writeToClients((const void*)(&value), sizeof(CompassData));
}
