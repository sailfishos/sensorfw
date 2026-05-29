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

#ifndef CHOPCHOP_SENSOR_H
#define CHOPCHOP_SENSOR_H

#include <QtDBus/QtDBus>
#include <QObject>

#include "datatypes/unsigned.h"
#include "abstractsensor_a.h"

class ChopChopSensorChannelAdaptor : public AbstractSensorChannelAdaptor
{
    Q_OBJECT
    Q_DISABLE_COPY(ChopChopSensorChannelAdaptor)
    Q_CLASSINFO("D-Bus Interface", "local.ChopChopSensor")
    Q_PROPERTY(Unsigned chopChop READ chopChop NOTIFY chopChopChanged)

public:
    ChopChopSensorChannelAdaptor(QObject *parent);

public Q_SLOTS:
    Unsigned chopChop() const;

Q_SIGNALS:
    void chopChopChanged(const Unsigned &value);
};
#endif // CHOPCHOP_SENSOR_H
