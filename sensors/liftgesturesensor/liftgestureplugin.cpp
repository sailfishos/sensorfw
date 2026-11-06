/****************************************************************************
**
** Copyright (c) 2025 Jollyboys Ltd.
**
** $QT_BEGIN_LICENSE:LGPL$
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "liftgestureplugin.h"
#include "liftgesturesensor.h"
#include "sensormanager.h"
#include "logging.h"

void LiftGesturePlugin::Register(class Loader &l)
{
    Q_UNUSED(l);
    qCDebug(lcSensorFw) << "registering liftgesturesensor";
    SensorManager &sm = SensorManager::instance();
    sm.registerSensor<LiftGestureSensorChannel>("liftgesturesensor");
}

void LiftGesturePlugin::Init(class Loader &l)
{
    Q_UNUSED(l);
    SensorManager::instance().requestSensor("liftgesturesensor");
}

QStringList LiftGesturePlugin::Dependencies()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return QString("liftgestureadaptor").split(":", Qt::SkipEmptyParts);
#else
    return QString("liftgestureadaptor").split(":", QString::SkipEmptyParts);
#endif
}
