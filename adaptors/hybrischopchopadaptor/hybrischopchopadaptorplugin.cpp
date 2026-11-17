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

#include "hybrischopchopadaptorplugin.h"
#include "hybrischopchopadaptor.h"
#include "sensormanager.h"
#include "logging.h"

void HybrisChopChopAdaptorPlugin::Register(class Loader &l)
{
    Q_UNUSED(l);
    qCDebug(lcSensorFw) << "registering hybrischopchopadaptor";
    SensorManager &sm = SensorManager::instance();
    sm.registerDeviceAdaptor<HybrisChopChopAdaptor>("chopchopadaptor");
}
