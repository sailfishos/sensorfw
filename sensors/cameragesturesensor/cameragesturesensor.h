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

#ifndef CAMERAGESTURE_SENSOR_CHANNEL_H
#define CAMERAGESTURE_SENSOR_CHANNEL_H

#include <QObject>

#include "deviceadaptor.h"
#include "abstractsensor.h"
#include "cameragesturesensor_a.h"
#include "dataemitter.h"
#include "datatypes/timedunsigned.h"
#include "datatypes/unsigned.h"

class Bin;
template <class TYPE> class BufferReader;
class FilterBase;

/** Sensor for accessing cameragesture sensor events.
 *
 * Signals whenever cameragesture sensor events are received.
 */
class CameraGestureSensorChannel
    : public AbstractSensorChannel
    , public DataEmitter<TimedUnsigned>
{
    Q_OBJECT
    Q_PROPERTY(Unsigned cameraGesture READ cameraGesture NOTIFY cameraGestureChanged)

public:
    /** Factory method for CameraGestureSensorChannel.
     *
     * @return New CameraGestureSensorChannel as AbstractSensorChannel*
     */
    static AbstractSensorChannel *factoryMethod(const QString &id)
    {
        CameraGestureSensorChannel *sc = new CameraGestureSensorChannel(id);
        new CameraGestureSensorChannelAdaptor(sc);

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
    Unsigned cameraGesture() const { return m_previousValue; }

public Q_SLOTS:
    bool start();
    bool stop();

signals:
    /** Sent when a change in measured data is observed.
     *
     * @param value Measured value.
     */
    void cameraGestureChanged(const Unsigned &value);

protected:
    CameraGestureSensorChannel(const QString &id);
    virtual ~CameraGestureSensorChannel();

private:
    TimedUnsigned m_previousValue;
    Bin *m_filterBin;
    Bin *m_marshallingBin;
    DeviceAdaptor *m_cameraGestureAdaptor;
    BufferReader<TimedUnsigned> *m_cameraGestureReader;
    RingBuffer<TimedUnsigned> *m_outputBuffer;

    void emitData(const TimedUnsigned &value);
};
#endif // CAMERAGESTURE_SENSOR_CHANNEL_H
