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

#include "wakeupsensor_a.h"

WakeupSensorChannelAdaptor::WakeupSensorChannelAdaptor(QObject *parent)
    : AbstractSensorChannelAdaptor(parent)
{
}

Unsigned WakeupSensorChannelAdaptor::wakeup() const
{
    return qvariant_cast<Unsigned>(parent()->property("wakeup"));
}
