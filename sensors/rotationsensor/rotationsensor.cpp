/**
   @file rotationsensor.cpp
   @brief RotationSensor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
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

#include "rotationsensor.h"
#include <QMutexLocker>
#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"

RotationSensorChannel::RotationSensorChannel(const QString& id) :
        AbstractSensorChannel(id),
        DataEmitter<TimedXyzData>(1),
        compassReader_(NULL),
        prevRotation_(0,0,0,0)
{
    SensorManager& sm = SensorManager::instance();

    accelerometerChain_ = sm.requestChain("accelerometerchain");
    if (!accelerometerChain_) {
        setValid(false);
        return;
    }

    accelerometerReader_ = new BufferReader<AccelerationData>(1);

    compassChain_ = sm.requestChain("compasschain");
    if (compassChain_ && compassChain_->isValid()) {
        compassReader_ = new BufferReader<CompassData>(1);
    } else {
        qCWarning(lcSensorFw) << NodeBase::id() << "Unable to use compass for z-axis rotation.";
    }

    rotationFilter_ = sm.instantiateFilter("rotationfilter");
    if (!rotationFilter_) {
        setValid(false);
        return;
    }
    setValid(true);

    outputBuffer_ = new RingBuffer<TimedXyzData>(1);

    // Create buffers for filter chain
    filterBin_ = new Bin;

    filterBin_->add(accelerometerReader_, "accelerometer");
    filterBin_->add(rotationFilter_, "rotationfilter");
    filterBin_->add(outputBuffer_, "buffer");

    if (hasZ()) {
        filterBin_->add(compassReader_, "compass");
        filterBin_->join("compass", "source", "rotationfilter", "compasssink");
    }

    filterBin_->join("accelerometer", "source", "rotationfilter", "accelerometersink");
    filterBin_->join("rotationfilter", "source", "buffer", "sink");

    connectToSource(accelerometerChain_, "accelerometer", accelerometerReader_);

    if (hasZ()) {
        connectToSource(compassChain_, "truenorth", compassReader_);
        addStandbyOverrideSource(compassChain_);
    }

    marshallingBin_ = new Bin;
    marshallingBin_->add(this, "sensorchannel");

    outputBuffer_->join(this);

    setDescription("x, y, and z axes rotation in degrees");
    introduceAvailableDataRange(DataRange(-179, 180, 1));
    addStandbyOverrideSource(accelerometerChain_);

    // Provide interval value from acc, but range depends on sane compass
    if (hasZ()) {
        // No less than 5hz allowed for compass
        int ranges_ms[] = {10, 20, 25, 40, 50, 100, 200};
        for (size_t i = 0; i < sizeof(ranges_ms) / sizeof(int); ++i) {
            int range_us = ranges_ms[i] * 1000;
            introduceAvailableInterval(DataRange(range_us, range_us, 0));
        }
    } else {
        setIntervalSource(accelerometerChain_);
    }

    // Tricky. Might need to make this conditional.
    unsigned int interval_us = 100 * 1000;
    setDefaultInterval(interval_us);
}

RotationSensorChannel::~RotationSensorChannel()
{
    if (isValid()) {
        SensorManager& sm = SensorManager::instance();

        disconnectFromSource(accelerometerChain_, "accelerometer", accelerometerReader_);
        sm.releaseChain("accelerometerchain");

        if (hasZ()) {
            disconnectFromSource(compassChain_, "truenorth", compassReader_);
            sm.releaseChain("compasschain");
            delete compassReader_;
        }

        delete accelerometerReader_;
        delete rotationFilter_;
        delete outputBuffer_;
        delete marshallingBin_;
        delete filterBin_;
    }
}

bool RotationSensorChannel::start()
{
    qCInfo(lcSensorFw) << id() << "Starting RotationSensorChannel";

    if (AbstractSensorChannel::start()) {
        marshallingBin_->start();
        filterBin_->start();
        accelerometerChain_->start();
        if (hasZ()) {
            compassChain_->setProperty("compassEnabled", true);
            compassChain_->start();
        }
    }
    return true;
}

bool RotationSensorChannel::stop()
{
    qCInfo(lcSensorFw) << id() << "Stopping RotationSensorChannel";

    if (AbstractSensorChannel::stop()) {
        accelerometerChain_->stop();
        filterBin_->stop();
        if (hasZ()) {
            compassChain_->stop();
            compassChain_->setProperty("compassEnabled", false);
        }
        marshallingBin_->stop();
    }
    return true;
}

void RotationSensorChannel::emitData(const TimedXyzData& value)
{
    QMutexLocker locker(&mutex_);

    prevRotation_ = value;
    downsampleAndPropagate(value, downsampleBuffer_);
}

unsigned int RotationSensorChannel::interval() const
{
    // Just provide accelerometer rate for now.
    return accelerometerChain_->getInterval();
}

bool RotationSensorChannel::setInterval(int sessionId, unsigned int interval_us)
{
    bool success = accelerometerChain_->setIntervalRequest(sessionId, interval_us);
    if (hasZ()) {
        success = compassChain_->setIntervalRequest(sessionId, interval_us) && success;
    }

    return success;
}

void RotationSensorChannel::removeSession(int sessionId)
{
    downsampleBuffer_.remove(sessionId);
    AbstractSensorChannel::removeSession(sessionId);
}

bool RotationSensorChannel::downsamplingSupported() const
{
    return true;
}
