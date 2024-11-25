/**
   @file abstractsensor.cpp
   @brief Base class for sensors

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Semi Malinen <semi.malinen@nokia.com
   @author Joep van Gassel <joep.van.gassel@nokia.com>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Antti Virtanen <antti.i.virtanen@nokia.com>
   @author Shenghua Liu <ext-shenghua.1.liu@nokia.com>

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

#include "abstractsensor.h"
#include "sensormanager.h"
#include "sockethandler.h"
#include "idutils.h"
#include "logging.h"

AbstractSensorChannel::AbstractSensorChannel(const QString& id) :
    NodeBase(getCleanId(id)),
    errorCode_(SNoError),
    cnt_(0)
{
}

void AbstractSensorChannel::setError(SensorError errorCode, const QString& errorString)
{
    qCCritical(lcSensorFw) << id() << "SensorError: " <<  errorString;

    errorCode_   = errorCode;
    errorString_ = errorString;

    emit errorSignal(errorCode);
}

bool AbstractSensorChannel::start(int sessionId)
{
    if (!activeSessions_.contains(sessionId)) {
        activeSessions_.insert(sessionId);
        requestDefaultInterval(sessionId);
        return start();
    }
    return false;
}

bool AbstractSensorChannel::start()
{
    return ++cnt_ == 1;
}

bool AbstractSensorChannel::stop(int sessionId)
{
    if (activeSessions_.remove(sessionId)) {
        // Note: when client restarts the session it is responsible to reconfiguring the sensor.
        removeSession(sessionId);
        return stop();
    }
    return false;
}

bool AbstractSensorChannel::stop()
{
    if (--cnt_ == 0)
        return true;
    if (cnt_ < 0)
        cnt_ = 0;
    return false;
}

bool AbstractSensorChannel::writeToSession(int sessionId, const void* source, int size)
{
    if (!(SensorManager::instance().write(sessionId, source, size))) {
        qCInfo(lcSensorFw) << id() << "AbstractSensor failed to write to session " << sessionId;
        return false;
    }
    return true;
}

bool AbstractSensorChannel::writeToClients(const void* source, int size)
{
    bool ret = true;
    foreach (int sessionId, activeSessions_) {
        ret &= writeToSession(sessionId, source, size);
    }
    return ret;
}

bool AbstractSensorChannel::downsampleAndPropagate(const TimedXyzData& data, TimedXyzDownsampleBuffer& buffer)
{
    bool ret = true;
    unsigned int currentInterval = getInterval();

    foreach (int sessionId, activeSessions_) {
        if (!downsamplingEnabled(sessionId)) {
            ret &= writeToSession(sessionId, (const void *)& data, sizeof(TimedXyzData));
            continue;
        }
        unsigned int sessionInterval = getInterval(sessionId);
        int bufferSize = (sessionInterval < currentInterval || !currentInterval) ? 1 : sessionInterval / currentInterval;

        QList<TimedXyzData>& samples(buffer[sessionId]);
        samples.push_back(data);

        for (QList<TimedXyzData>::iterator it = samples.begin(); it != samples.end(); ++it) {
            if (samples.size() > bufferSize
                    || data.timestamp_ - it->timestamp_ > 2000000) {
                it = samples.erase(it);
                if (it == samples.end())
                    break;
            } else {
                break;
            }
        }

        if (samples.size() < bufferSize)
            continue;

        float x = 0;
        float y = 0;
        float z = 0;

        foreach (const TimedXyzData& data, samples) {
            x += data.x_;
            y += data.y_;
            z += data.z_;
        }
        TimedXyzData downsampled(data.timestamp_,
                                 x / samples.count(),
                                 y / samples.count(),
                                 z / samples.count());

        if (writeToSession(sessionId, (const void*)& downsampled, sizeof(TimedXyzData))) {
            samples.clear();
        } else {
            ret = false;
        }
    }

    return ret;
}

bool AbstractSensorChannel::downsampleAndPropagate(const CalibratedMagneticFieldData& data, MagneticFieldDownsampleBuffer& buffer)
{
    bool ret = true;
    unsigned int currentInterval = getInterval();

    foreach (int sessionId, activeSessions_) {
        if (!downsamplingEnabled(sessionId)) {
            ret &= writeToSession(sessionId, (const void *)& data, sizeof(CalibratedMagneticFieldData));
            continue;
        }
        unsigned int sessionInterval = getInterval(sessionId);
        int bufferSize = (sessionInterval < currentInterval || !currentInterval) ? 1 : sessionInterval / currentInterval;

        QList<CalibratedMagneticFieldData>& samples(buffer[sessionId]);
        samples.push_back(data);

        for (QList<CalibratedMagneticFieldData>::iterator it = samples.begin(); it != samples.end(); ++it) {
            if (samples.size() > bufferSize
                    || data.timestamp_ - it->timestamp_ > 2000000) {
                it = samples.erase(it);
                if (it == samples.end())
                    break;
            } else {
                break;
            }
        }

        if (samples.size() < bufferSize)
            continue;

        long x = 0;
        long y = 0;
        long z = 0;
        long rx = 0;
        long ry = 0;
        long rz = 0;

        foreach (const CalibratedMagneticFieldData& data, samples) {
            x += data.x_;
            y += data.y_;
            z += data.z_;
            rx += data.rx_;
            ry += data.ry_;
            rz += data.rz_;
        }
        CalibratedMagneticFieldData downsampled(data.timestamp_,
                                                x / samples.count(),
                                                y / samples.count(),
                                                z / samples.count(),
                                                rx / samples.count(),
                                                ry / samples.count(),
                                                rz / samples.count(),
                                                data.level_);

        if (writeToSession(sessionId, (const void*)& downsampled, sizeof(CalibratedMagneticFieldData))) {
            samples.clear();
        } else {
            ret = false;
        }

    }
    return ret;
}


void AbstractSensorChannel::setDownsamplingEnabled(int sessionId, bool value)
{
    if (downsamplingSupported()) {
        qCDebug(lcSensorFw) << id() << "Downsampling state for session " << sessionId << ": " << value;
        downsampling_[sessionId] = value;
    }
}

bool AbstractSensorChannel::downsamplingEnabled(int sessionId) const
{
    QMap<int, bool>::const_iterator it(downsampling_.find(sessionId));
    if (it == downsampling_.end())
        return downsamplingSupported();
    return it.value() && getInterval(sessionId);
}

bool AbstractSensorChannel::downsamplingSupported() const
{
    return false;
}

void AbstractSensorChannel::removeSession(int sessionId)
{
    downsampling_.take(sessionId);
    NodeBase::removeSession(sessionId);
}

SensorError AbstractSensorChannel::errorCode() const
{
    return errorCode_;
}

const QString& AbstractSensorChannel::errorString() const
{
    return errorString_;
}

const QString AbstractSensorChannel::type() const
{
    return metaObject()->className();
}

bool AbstractSensorChannel::running() const
{
    return (cnt_ > 0);
}

void AbstractSensorChannel::clearError()
{
    errorCode_ = SNoError;
    errorString_.clear();
}

void AbstractSensorChannel::signalPropertyChanged(const QString& name)
{
    emit propertyChanged(name);
}

RingBufferBase* AbstractSensorChannel::findBuffer(const QString&) const
{
    qCWarning(lcSensorFw) << id() << "Tried to locate buffer from SensorChannel!";
    return nullptr;
}
