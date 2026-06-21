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
 * Adaptor for chopChop sensor. Provides chopChop events that can be used
 * for example for triggering SneakPeek mode. Sensor value of one means
 * chopChop condition was met. Other values should be ignored. Uses hybris
 * sensor daemon driver interface.
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
