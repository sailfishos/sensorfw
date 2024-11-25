/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd

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
 */

#include "magcalibrationchainplugin.h"
#include "magcalibrationchain.h"
#include "calibrationfilter.h"
#include "sensormanager.h"
#include "logging.h"

void MagCalibrationChainPlugin::Register(class Loader&)
{
    qCInfo(lcSensorFw) << "registering magcalibrationchain";
    SensorManager& sm = SensorManager::instance();
    sm.registerFilter<CalibrationFilter>("calibrationfilter");
    sm.registerChain<MagCalibrationChain>("magcalibrationchain");
}

QStringList MagCalibrationChainPlugin::Dependencies() {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return QString("magcoordinatealignfilter:magnetometeradaptor").split(":", Qt::SkipEmptyParts);
#else
    return QString("magcoordinatealignfilter:magnetometeradaptor").split(":", QString::SkipEmptyParts);
#endif
}
