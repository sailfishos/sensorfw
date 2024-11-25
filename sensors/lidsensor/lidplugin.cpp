/**
   @file lidplugin.cpp
   @brief Plugin for LidSensor

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

#include "lidplugin.h"
#include "lidsensor.h"
#include "sensormanager.h"
#include "logging.h"

void LidPlugin::Register(class Loader&)
{
    qCInfo(lcSensorFw) << "registering lidsensor";
    SensorManager& sm = SensorManager::instance();
    sm.registerSensor<LidSensorChannel>("lidsensor");
}

void LidPlugin::Init(class Loader& l)
{
    Q_UNUSED(l);
    SensorManager::instance().requestSensor("lidsensor");
}

QStringList LidPlugin::Dependencies() {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return QString("lidsensoradaptor").split(":", Qt::SkipEmptyParts);
#else
    return QString("lidsensoradaptor").split(":", QString::SkipEmptyParts);
#endif
}
