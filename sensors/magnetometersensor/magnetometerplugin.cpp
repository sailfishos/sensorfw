/**
   @file magnetometerplugin.cpp
   @brief Plugin for MagnetometerSensor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

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

#include "magnetometerplugin.h"
#include "magnetometersensor.h"
#include "magnetometerscalefilter.h"
#include "sensormanager.h"
#include "logging.h"

void MagnetometerPlugin::Register(class Loader&)
{
    SensorManager& sm = SensorManager::instance();
    sm.registerSensor<MagnetometerSensorChannel>("magnetometersensor");
    sm.registerFilter<MagnetometerScaleFilter>("magnetometerscalefilter");
}

QStringList MagnetometerPlugin::Dependencies() {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return QString("magcalibrationchain").split(":", Qt::SkipEmptyParts);
#else
    return QString("magcalibrationchain").split(":", QString::SkipEmptyParts);
#endif
}
