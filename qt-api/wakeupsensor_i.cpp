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

#include "sensormanagerinterface.h"
#include "wakeupsensor_i.h"
#include "socketreader.h"

const char *WakeupSensorChannelInterface::staticInterfaceName = "local.WakeupSensor";

AbstractSensorChannelInterface *WakeupSensorChannelInterface::factoryMethod(const QString &id, int sessionId)
{
    return new WakeupSensorChannelInterface(OBJECT_PATH + "/" + id, sessionId);
}

WakeupSensorChannelInterface::WakeupSensorChannelInterface(const QString &path, int sessionId)
    : AbstractSensorChannelInterface(path, WakeupSensorChannelInterface::staticInterfaceName, sessionId)
{
}

WakeupSensorChannelInterface *WakeupSensorChannelInterface::interface(const QString &id)
{
    SensorManagerInterface &sm = SensorManagerInterface::instance();
    if (!sm.registeredAndCorrectClassName(id, WakeupSensorChannelInterface::staticMetaObject.className()))
        return nullptr;
    return dynamic_cast<WakeupSensorChannelInterface *>(sm.interface(id));
}

bool WakeupSensorChannelInterface::dataReceivedImpl()
{
    QVector<TimedUnsigned> values;
    if (!read<TimedUnsigned>(values))
        return false;
    foreach (const TimedUnsigned &data, values)
        emit wakeupChanged(data);
    return true;
}

Unsigned WakeupSensorChannelInterface::wakeup()
{
    return getAccessor<Unsigned>("wakeup");
}
