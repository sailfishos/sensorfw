/**
   @file lidsensoradaptor-evdevplugin.cpp
   @brief Plugin for LidSensorAdaptorEvdev

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

#include "lidsensoradaptor-evdevplugin.h"
#include "lidsensoradaptor-evdev.h"
#include "sensormanager.h"
#include "logging.h"

void LidsensorAdaptorEvdevPlugin::Register(class Loader&)
{
    qCInfo(lcSensorFw) << "registering lidsensoradaptor-evdev";
    SensorManager& sm = SensorManager::instance();
    sm.registerDeviceAdaptor<LidSensorAdaptorEvdev>("lidsensoradaptor");
}
