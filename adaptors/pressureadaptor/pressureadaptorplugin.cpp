/**
   @file pressureadaptorplugin.cpp
   @brief Plugin for PressureAdaptor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation
   Copyright (C) 2015 Jolla

   @author Lorn Potter <lorn.potter@jolla.com>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Markus Lehtonen <markus.lehtonen@nokia.com>

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

#include "pressureadaptorplugin.h"
#include "pressureadaptor.h"
#include "sensormanager.h"
#include "logging.h"

void PressureAdaptorPlugin::Register(class Loader&)
{
    qCInfo(lcSensorFw) << "registering pressureadaptor";
    SensorManager& sm = SensorManager::instance();
    sm.registerDeviceAdaptor<PressureAdaptor>("pressureadaptor");
}
