/**
   @file stepcounterplugin.cpp
   @brief Plugin for stepcounter sensor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation
   Copyright (C) 2016 kimmoli

   @author Kimmo Lindholm <kimmo.lindholm@eke.fi>
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

#include "stepcounterplugin.h"
#include "stepcountersensor.h"
#include "sensormanager.h"
#include "logging.h"

void StepCounterPlugin::Register(class Loader&)
{
    sensordLogD() << "registering stepcountersensor";
    SensorManager& sm = SensorManager::instance();
    sm.registerSensor<StepCounterSensorChannel>("stepcountersensor");
}

void StepCounterPlugin::Init(class Loader& l)
{
    Q_UNUSED(l);
    SensorManager::instance().requestSensor("stepcountersensor");
}

QStringList StepCounterPlugin::Dependencies() {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return QString("stepcounteradaptor").split(":", Qt::SkipEmptyParts);
#else
    return QString("stepcounteradaptor").split(":", QString::SkipEmptyParts);
#endif
}
