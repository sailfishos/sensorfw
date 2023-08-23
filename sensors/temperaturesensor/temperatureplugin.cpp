/**
   @file temperatureplugin.cpp
   @brief Plugin for TemperatureSensor

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

#include "temperatureplugin.h"
#include "temperaturesensor.h"
#include "sensormanager.h"
#include "logging.h"

void TemperaturePlugin::Register(class Loader&)
{
    sensordLogD() << "registering temperaturesensor";
    SensorManager& sm = SensorManager::instance();
    sm.registerSensor<TemperatureSensorChannel>("temperaturesensor");
}

void TemperaturePlugin::Init(class Loader& l)
{
    Q_UNUSED(l);
    SensorManager::instance().requestSensor("temperaturesensor");
}

QStringList TemperaturePlugin::Dependencies() {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return QString("temperatureadaptor").split(":", Qt::SkipEmptyParts);
#else
    return QString("temperatureadaptor").split(":", QString::SkipEmptyParts);
#endif
}
