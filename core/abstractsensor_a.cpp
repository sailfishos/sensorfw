/**
   @file abstractsensor_a.cpp
   @brief D-Bus adaptor base class for sensors

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Semi Malinen <semi.malinen@nokia.com
   @author Joep van Gassel <joep.van.gassel@nokia.com>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
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

#include "abstractsensor_a.h"
#include "sfwerror.h"
#include <sensormanager.h>
#include <sockethandler.h>

AbstractSensorChannelAdaptor::AbstractSensorChannelAdaptor(QObject *parent) :
    QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(false); //disabling signals since no public client API supports the use of these
}

bool AbstractSensorChannelAdaptor::isValid() const
{
    return node()->isValid();
}

int AbstractSensorChannelAdaptor::errorCodeInt() const
{
    return static_cast<int>(node()->errorCode());
}

QString AbstractSensorChannelAdaptor::errorString() const
{
    return node()->errorString();
}

QString AbstractSensorChannelAdaptor::description() const
{
    return node()->description();
}

QString AbstractSensorChannelAdaptor::id() const
{
    return node()->id();
}

unsigned int AbstractSensorChannelAdaptor::interval() const
{
    // D-Bus interface -> interval is milliseconds
    int interval_ms = 0;
    int interval_us = node()->getInterval();
    if (interval_us > 0)
        interval_ms = (interval_us + 999) / 1000;
    return interval_ms;
}

unsigned int AbstractSensorChannelAdaptor::bufferInterval() const
{
    // D-Bus interface -> interval is milliseconds
    int interval_ms = 0;
    int interval_us = node()->bufferInterval();
    if (interval_us > 0)
        interval_ms = (interval_us + 999) / 1000;
    return interval_ms;
}

unsigned int AbstractSensorChannelAdaptor::bufferSize() const
{
    return node()->bufferSize();
}

bool AbstractSensorChannelAdaptor::hwBuffering() const
{
    bool hwBuffering = false;
    node()->getAvailableBufferSizes(hwBuffering);
    return hwBuffering;
}

QString AbstractSensorChannelAdaptor::type() const
{
    return node()->type();
}

void AbstractSensorChannelAdaptor::start(int sessionId)
{
    node()->start(sessionId);
}

void AbstractSensorChannelAdaptor::stop(int sessionId)
{
    node()->stop(sessionId);
}

void AbstractSensorChannelAdaptor::setInterval(int sessionId, int interval_ms)
{
    // D-Bus interface -> interval is milliseconds
    int interval_us = 0;
    if (interval_ms > 0)
        interval_us = interval_ms * 1000;
    node()->setIntervalRequest(sessionId, interval_us);
    SensorManager::instance().socketHandler().setInterval(sessionId, interval_us);
}

void AbstractSensorChannelAdaptor::setDataRate(int sessionId, double dataRate_Hz)
{
    // interval_us rounded down -> effective dataRate rounded up
    int interval_us = 0;
    if (dataRate_Hz > 0)
        interval_us = (int)(1000000.0 / dataRate_Hz);
    node()->setIntervalRequest(sessionId, interval_us);
    SensorManager::instance().socketHandler().setInterval(sessionId, interval_us);
}

bool AbstractSensorChannelAdaptor::standbyOverride() const
{
    return node()->standbyOverride();
}

bool AbstractSensorChannelAdaptor::setStandbyOverride(int sessionId, bool value)
{
    return node()->setStandbyOverrideRequest(sessionId, value);
}

DataRangeList AbstractSensorChannelAdaptor::getAvailableDataRanges()
{
    return node()->getAvailableDataRanges();
}

DataRange AbstractSensorChannelAdaptor::getCurrentDataRange()
{
    return node()->getCurrentDataRange().range;
}

void AbstractSensorChannelAdaptor::requestDataRange(int sessionId, DataRange range)
{
    node()->requestDataRange(sessionId, range);
}

void AbstractSensorChannelAdaptor::removeDataRangeRequest(int sessionId)
{
    node()->removeDataRangeRequest(sessionId);
}

DataRangeList AbstractSensorChannelAdaptor::getAvailableIntervals()
{
    // D-Bus interface -> interval is milliseconds
    DataRangeList ranges_us(node()->getAvailableIntervals());
    DataRangeList ranges_ms;
    for (auto it = ranges_us.begin(); it != ranges_us.end(); ++it)
        ranges_ms.append(DataRange(it->min * 0.001, it->max * 0.001, it->resolution));
    return ranges_ms;
}

bool AbstractSensorChannelAdaptor::setDefaultInterval(int sessionId)
{
    bool ok = node()->requestDefaultInterval(sessionId);
    SensorManager::instance().socketHandler().clearInterval(sessionId);
    return ok;
}

void AbstractSensorChannelAdaptor::setBufferInterval(int sessionId, unsigned int interval_ms)
{
    // D-Bus interface -> interval is milliseconds
    int interval_us = 0;
    if (interval_ms > 0)
        interval_us = interval_ms * 1000;
    bool hwBuffering = false;
    node()->getAvailableBufferIntervals(hwBuffering);
    if (hwBuffering) {
        if (interval_us == 0)
            node()->clearBufferInterval(sessionId);
        else
            node()->setBufferInterval(sessionId, interval_us);
        interval_us = 0;
    }
    if (interval_us == 0)
        SensorManager::instance().socketHandler().clearBufferInterval(sessionId);
    else
        SensorManager::instance().socketHandler().setBufferInterval(sessionId, interval_us);
}

void AbstractSensorChannelAdaptor::setBufferSize(int sessionId, unsigned int value)
{
    bool hwBuffering = false;
    node()->getAvailableBufferSizes(hwBuffering);
    if (hwBuffering) {
        if (value == 0)
            node()->clearBufferSize(sessionId);
        else
            node()->setBufferSize(sessionId, value);
    }
    if (value == 0)
        SensorManager::instance().socketHandler().clearBufferSize(sessionId);
    else
        SensorManager::instance().socketHandler().setBufferSize(sessionId, value);
}

IntegerRangeList AbstractSensorChannelAdaptor::getAvailableBufferIntervals() const
{
    // D-Bus interface -> interval is milliseconds
    bool dummy;
    IntegerRangeList list(node()->getAvailableBufferIntervals(dummy));
    for (auto it = list.begin(); it != list.end(); ++it) {
        (*it).first = ((*it).first + 999) / 1000;
        (*it).second = ((*it).second + 999) / 1000;
    }
    return list;
}

IntegerRangeList AbstractSensorChannelAdaptor::getAvailableBufferSizes() const
{
    bool dummy;
    return node()->getAvailableBufferSizes(dummy);
}

AbstractSensorChannel* AbstractSensorChannelAdaptor::node() const
{
    return dynamic_cast<AbstractSensorChannel*>(parent());
}

bool AbstractSensorChannelAdaptor::setDataRangeIndex(int sessionId, int rangeIndex)
{
    return node()->setDataRangeIndex(sessionId, rangeIndex);
}

void AbstractSensorChannelAdaptor::setDownsampling(int sessionId, bool value)
{
    node()->setDownsamplingEnabled(sessionId, value);
}
