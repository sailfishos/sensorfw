/**
   @file humidityadaptorplugin.cpp
   @brief Plugin for HumidityAdaptor

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

#include "humidityadaptorplugin.h"
#include "humidityadaptor.h"
#include "sensormanager.h"
#include "logging.h"

void HumidityAdaptorPlugin::Register(class Loader&)
{
    qCInfo(lcSensorFw) << "registering humidityadaptor";
    SensorManager& sm = SensorManager::instance();
    sm.registerDeviceAdaptor<HumidityAdaptor>("humidityadaptor");
}
