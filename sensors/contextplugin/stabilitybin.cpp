/**
   @file stabilitybin.cpp
   @brief Stability Bin for ContextFW
   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Marja Hassinen <ext-marja.2.hassinen@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
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

#include "stabilitybin.h"
#include "contextplugin.h"
#include "sensormanager.h"
#include "config.h"
#include "logging.h"

const int StabilityBin::STABILITY_THRESHOLD = 7;
const int StabilityBin::UNSTABILITY_THRESHOLD = 300;
const float StabilityBin::STABILITY_HYSTERESIS = 0.1;

StabilityBin::StabilityBin(ContextProvider::Service& s):
    isStableProperty(s, "Position.Stable"),
    isShakyProperty(s, "Position.Shaky"),
    accelerometerReader(10),
    cutterFilter(4.0),
    avgVarFilter(60),
    stabilityFilter(&isStableProperty, &isShakyProperty, STABILITY_THRESHOLD, UNSTABILITY_THRESHOLD, STABILITY_HYSTERESIS),
    sessionId(0)
{
    add(&accelerometerReader, "accelerometer");
    add(&normalizerFilter, "normalizerfilter");
    add(&cutterFilter, "cutterfilter");
    add(&avgVarFilter, "avgvarfilter");
    add(&stabilityFilter, "stabilityfilter");

    join("accelerometer", "source", "normalizerfilter", "sink");
    join("normalizerfilter", "source", "cutterfilter", "sink");
    join("cutterfilter", "source", "avgvarfilter", "sink");
    join("avgvarfilter", "source", "stabilityfilter", "sink");

    // Context group
    group.add(isStableProperty);
    group.add(isShakyProperty);
    connect(&group, SIGNAL(firstSubscriberAppeared()), this, SLOT(startRun()));
    connect(&group, SIGNAL(lastSubscriberDisappeared()), this, SLOT(stopRun()));
}

StabilityBin::~StabilityBin()
{
    stopRun();
}

void StabilityBin::startRun()
{
    // Get unique sessionId for this Bin.
    sessionId = SensorManager::instance().requestSensor("contextsensor");
    if (sessionId == INVALID_SESSION)
    {
        qCCritical(lcSensorFw) << id() << "Failed to get unique sessionId for stability detection.";
    }

    accelerometerAdaptor = SensorManager::instance().requestDeviceAdaptor("accelerometeradaptor");
    if (!accelerometerAdaptor)
    {
        qCCritical(lcSensorFw) << id() << "Unable to access Accelerometer for stability properties.";
        return;
    }

    RingBufferBase* rb = accelerometerAdaptor->findBuffer("accelerometer");
    if (!rb)
    {
        qCCritical(lcSensorFw) << id() << "Unable to connect to accelerometer.";
    } else {
        rb->join(&accelerometerReader);
    }

    // Reset the status of the avg & var computation; reset default
    // values for properties whose values aren't reliable after a
    // restart
    avgVarFilter.reset();
    isStableProperty.unsetValue();
    isShakyProperty.unsetValue();
    start();
    accelerometerAdaptor->startSensor();
    accelerometerAdaptor->setStandbyOverrideRequest(sessionId, true);
}

void StabilityBin::stopRun()
{
    stop();
    if (accelerometerAdaptor)
    {
        accelerometerAdaptor->stopSensor();
        RingBufferBase* rb = accelerometerAdaptor->findBuffer("accelerometer");
        if (rb)
        {
            rb->unjoin(&accelerometerReader);
        }
        accelerometerAdaptor->removeSession(sessionId);
        SensorManager::instance().releaseDeviceAdaptor("accelerometeradaptor");
        accelerometerAdaptor = NULL;
    }
    SensorManager::instance().releaseSensor("contextsensor", sessionId);
}
