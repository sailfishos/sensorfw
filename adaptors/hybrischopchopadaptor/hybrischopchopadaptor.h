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

#ifndef HYBRISCHOPCHOPADAPTOR_H
#define HYBRISCHOPCHOPADAPTOR_H
#include "hybrisadaptor.h"

#include <QString>
#include <QStringList>
#include <QTime>
#include <linux/input.h>
#include "datatypes/timedunsigned.h"
#include "deviceadaptorringbuffer.h"

/** Adaptor for hybris chopChop sensor.
 *
 * Adaptor for chopchop sensor. Provides chopchop events that can be used
 * for toggling flashlight as that was the function on stock android.
 * Reported value is ignored if it arrives means it has been triggered.
 * The motion is difficult to describe because to some people chop chop
 * action is not clear, but its similar to vigoriously casting fishing rod
 * or hammering a nail. Available on Motorola phones only.
 */
class HybrisChopChopAdaptor : public HybrisAdaptor
{
    Q_OBJECT

public:
    static DeviceAdaptor *factoryMethod(const QString &id) {
        return new HybrisChopChopAdaptor(id);
    }
    HybrisChopChopAdaptor(const QString &id);
    ~HybrisChopChopAdaptor();

    bool startSensor();
    void stopSensor();

protected:
    void processSample(const sensors_event_t &data);

private:
    DeviceAdaptorRingBuffer<TimedUnsigned> *m_buffer;
    QByteArray m_powerStatePath;
};
#endif // HYBRISCHOPCHOPADAPTOR_H
