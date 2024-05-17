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

#ifndef WAKEUP_SENSOR_CHANNEL_H
#define WAKEUP_SENSOR_CHANNEL_H

#include <QObject>

#include "deviceadaptor.h"
#include "abstractsensor.h"
#include "wakeupsensor_a.h"
#include "dataemitter.h"
#include "datatypes/timedunsigned.h"
#include "datatypes/unsigned.h"

class Bin;
template <class TYPE> class BufferReader;
class FilterBase;

/** Sensor for accessing wakeup sensor events.
 *
 * Signals whenever wakeup sensor events are received.
 */
class WakeupSensorChannel
    : public AbstractSensorChannel
    , public DataEmitter<TimedUnsigned>
{
    Q_OBJECT
    Q_PROPERTY(Unsigned wakeup READ wakeup NOTIFY wakeupChanged)

public:
    /** Factory method for WakeupSensorChannel.
     *
     * @return New WakeupSensorChannel as AbstractSensorChannel*
     */
    static AbstractSensorChannel *factoryMethod(const QString &id)
    {
        WakeupSensorChannel *sc = new WakeupSensorChannel(id);
        new WakeupSensorChannelAdaptor(sc);

        return sc;
    }

    /** Property for accessing the measured value.
     *
     * Note that sensor does not have a state and thus
     * the last measured value is meaningness and is
     * provided only for the sake of symmetry.
     *
     * @return Last measured value.
     */
    Unsigned wakeup() const { return m_previousValue; }

public Q_SLOTS:
    bool start();
    bool stop();

signals:
    /** Sent when a change in measured data is observed.
     *
     * @param value Measured value.
     */
    void wakeupChanged(const Unsigned &value);

protected:
    WakeupSensorChannel(const QString &id);
    virtual ~WakeupSensorChannel();

private:
    TimedUnsigned m_previousValue;
    Bin *m_filterBin;
    Bin *m_marshallingBin;
    DeviceAdaptor *m_wakeupAdaptor;
    BufferReader<TimedUnsigned> *m_wakeupReader;
    RingBuffer<TimedUnsigned> *m_outputBuffer;

    void emitData(const TimedUnsigned &value);
};
#endif // WAKEUP_SENSOR_CHANNEL_H
