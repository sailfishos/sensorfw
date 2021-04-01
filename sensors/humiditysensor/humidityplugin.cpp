/**
   @file humidityplugin.cpp
   @brief Plugin for HumiditySensor

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

#include "humidityplugin.h"
#include "humiditysensor.h"
#include "sensormanager.h"
#include "logging.h"

void HumidityPlugin::Register(class Loader&)
{
    sensordLogD() << "registering humiditysensor";
    SensorManager& sm = SensorManager::instance();
    sm.registerSensor<HumiditySensorChannel>("humiditysensor");
}

void HumidityPlugin::Init(class Loader& l)
{
    Q_UNUSED(l);
    SensorManager::instance().requestSensor("humiditysensor");
}

QStringList HumidityPlugin::Dependencies() {
    return QString("humidityadaptor").split(":", Qt::SkipEmptyParts);
}
