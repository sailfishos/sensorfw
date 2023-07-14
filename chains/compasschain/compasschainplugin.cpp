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

#include "compasschainplugin.h"
#include "compasschain.h"
#include "compassfilter.h"
#include "orientationfilter.h"
#include "sensormanager.h"
#include "logging.h"
#include "config.h"

void CompassChainPlugin::Register(class Loader&)
{
    sensordLogD() << "registering compasschain";
    SensorManager& sm = SensorManager::instance();

    sm.registerChain<CompassChain>("compasschain");
    sm.registerFilter<CompassFilter>("compassfilter");
    sm.registerFilter<OrientationFilter>("orientationfilter");
}

QStringList CompassChainPlugin::Dependencies() {
    QByteArray orientationConfiguration = SensorFrameworkConfig::configuration()->value("plugins/orientationadaptor").toByteArray();
    if (orientationConfiguration.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        return QString("accelerometerchain:magcalibrationchain:declinationfilter:downsamplefilter:avgaccfilter").split(":", Qt::SkipEmptyParts);
#else
        return QString("accelerometerchain:magcalibrationchain:declinationfilter:downsamplefilter:avgaccfilter").split(":", QString::SkipEmptyParts);
#endif
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        return QString("accelerometerchain:magcalibrationchain:declinationfilter:downsamplefilter:avgaccfilter:orientationadaptor").split(":", Qt::SkipEmptyParts);
#else
        return QString("accelerometerchain:magcalibrationchain:declinationfilter:downsamplefilter:avgaccfilter:orientationadaptor").split(":", QString::SkipEmptyParts);
#endif
    }
}
