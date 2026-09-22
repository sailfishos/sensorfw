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

#ifndef CHOPCHOP_SENSOR_CHANNEL_H
#define CHOPCHOP_SENSOR_CHANNEL_H

#include <QObject>

#include "deviceadaptor.h"
#include "abstractsensor.h"
#include "chopchopsensor_a.h"
#include "dataemitter.h"
#include "datatypes/timedunsigned.h"
#include "datatypes/unsigned.h"

class Bin;
template <class TYPE> class BufferReader;
class FilterBase;

/** Sensor for accessing chopchop sensor events.
 *
 * Signals whenever chopchop sensor events are received.
 */
class ChopChopSensorChannel
    : public AbstractSensorChannel
    , public DataEmitter<TimedUnsigned>
{
    Q_OBJECT
    Q_PROPERTY(Unsigned chopChop READ chopChop NOTIFY chopChopChanged)

public:
    /** Factory method for ChopChopSensorChannel.
     *
     * @return New ChopChopSensorChannel as AbstractSensorChannel*
     */
    static AbstractSensorChannel *factoryMethod(const QString &id)
    {
        ChopChopSensorChannel *sc = new ChopChopSensorChannel(id);
        new ChopChopSensorChannelAdaptor(sc);

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
    Unsigned chopChop() const { return m_previousValue; }

public Q_SLOTS:
    bool start();
    bool stop();

signals:
    /** Sent when a change in measured data is observed.
     *
     * @param value Measured value.
     */
    void chopChopChanged(const Unsigned &value);

protected:
    ChopChopSensorChannel(const QString &id);
    virtual ~ChopChopSensorChannel();

private:
    TimedUnsigned m_previousValue;
    Bin *m_filterBin;
    Bin *m_marshallingBin;
    DeviceAdaptor *m_chopChopAdaptor;
    BufferReader<TimedUnsigned> *m_chopChopReader;
    RingBuffer<TimedUnsigned> *m_outputBuffer;

    void emitData(const TimedUnsigned &value);
};
#endif // CHOPCHOP_SENSOR_CHANNEL_H
