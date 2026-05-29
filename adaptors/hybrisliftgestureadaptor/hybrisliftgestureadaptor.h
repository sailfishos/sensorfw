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

#ifndef HYBRISLIFTGESTUREADAPTOR_H
#define HYBRISLIFTGESTUREADAPTOR_H
#include "hybrisadaptor.h"

#include <QString>
#include <QStringList>
#include <QTime>
#include <linux/input.h>
#include "datatypes/timedunsigned.h"
#include "deviceadaptorringbuffer.h"

/** Adaptor for hybris liftGesture sensor.
 *
 * Adaptor for liftGesture sensor. Provides liftGesture events that can be used
 * for example for triggering SneakPeek mode. Sensor value of one means
 * liftGesture condition was met. Other values should be ignored. Uses hybris
 * sensor daemon driver interface.
 */
class HybrisLiftGestureAdaptor : public HybrisAdaptor
{
    Q_OBJECT

public:
    static DeviceAdaptor *factoryMethod(const QString &id) {
        return new HybrisLiftGestureAdaptor(id);
    }
    HybrisLiftGestureAdaptor(const QString &id);
    ~HybrisLiftGestureAdaptor();

    bool startSensor();
    void stopSensor();

protected:
    void processSample(const sensors_event_t &data);

private:
    DeviceAdaptorRingBuffer<TimedUnsigned> *m_buffer;
    QByteArray m_powerStatePath;
};
#endif // HYBRISLIFTGESTUREADAPTOR_H
