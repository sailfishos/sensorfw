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

#ifndef LIFTGESTURE_SENSOR_CHANNEL_H
#define LIFTGESTURE_SENSOR_CHANNEL_H

#include <QObject>

#include "deviceadaptor.h"
#include "abstractsensor.h"
#include "liftgesturesensor_a.h"
#include "dataemitter.h"
#include "datatypes/timedunsigned.h"
#include "datatypes/unsigned.h"

class Bin;
template <class TYPE> class BufferReader;
class FilterBase;

/** Sensor for accessing liftgesture sensor events.
 *
 * Signals whenever liftgesture sensor events are received.
 */
class LiftGestureSensorChannel
    : public AbstractSensorChannel
    , public DataEmitter<TimedUnsigned>
{
    Q_OBJECT
    Q_PROPERTY(Unsigned liftGesture READ liftGesture NOTIFY liftGestureChanged)

public:
    /** Factory method for LiftGestureSensorChannel.
     *
     * @return New LiftGestureSensorChannel as AbstractSensorChannel*
     */
    static AbstractSensorChannel *factoryMethod(const QString &id)
    {
        LiftGestureSensorChannel *sc = new LiftGestureSensorChannel(id);
        new LiftGestureSensorChannelAdaptor(sc);

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
    Unsigned liftGesture() const { return m_previousValue; }

public Q_SLOTS:
    bool start();
    bool stop();

signals:
    /** Sent when a change in measured data is observed.
     *
     * @param value Measured value.
     */
    void liftGestureChanged(const Unsigned &value);

protected:
    LiftGestureSensorChannel(const QString &id);
    virtual ~LiftGestureSensorChannel();

private:
    TimedUnsigned m_previousValue;
    Bin *m_filterBin;
    Bin *m_marshallingBin;
    DeviceAdaptor *m_liftGestureAdaptor;
    BufferReader<TimedUnsigned> *m_liftGestureReader;
    RingBuffer<TimedUnsigned> *m_outputBuffer;

    void emitData(const TimedUnsigned &value);
};
#endif // LIFTGESTURE_SENSOR_CHANNEL_H
