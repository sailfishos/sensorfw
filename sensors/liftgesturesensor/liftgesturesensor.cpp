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

#include "liftgesturesensor.h"

#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"

LiftGestureSensorChannel::LiftGestureSensorChannel(const QString &id)
    : AbstractSensorChannel(id)
    , DataEmitter<TimedUnsigned>(1)
    , m_previousValue(0, 0)
    , m_filterBin(nullptr)
    , m_marshallingBin(nullptr)
    , m_liftGestureAdaptor(nullptr)
    , m_liftGestureReader(nullptr)
    , m_outputBuffer(nullptr)
{
    SensorManager &sm = SensorManager::instance();

    m_liftGestureAdaptor = sm.requestDeviceAdaptor("liftgestureadaptor");
    if (!m_liftGestureAdaptor) {
        setValid(false);
        return;
    }

    m_liftGestureReader = new BufferReader<TimedUnsigned>(1);

    m_outputBuffer = new RingBuffer<TimedUnsigned>(1);

    // Create buffers for filter chain
    m_filterBin = new Bin;

    m_filterBin->add(m_liftGestureReader, "liftgesture");
    m_filterBin->add(m_outputBuffer, "buffer");

    m_filterBin->join("liftgesture", "source", "buffer", "sink");

    // Join datasources to the chain
    connectToSource(m_liftGestureAdaptor, "liftgesture", m_liftGestureReader);

    m_marshallingBin = new Bin;
    m_marshallingBin->add(this, "sensorchannel");

    m_outputBuffer->join(this);

    setDescription("liftgesture events");
    setRangeSource(m_liftGestureAdaptor);
    addStandbyOverrideSource(m_liftGestureAdaptor);
    setIntervalSource(m_liftGestureAdaptor);

    setValid(true);
}

LiftGestureSensorChannel::~LiftGestureSensorChannel()
{
    if (m_liftGestureAdaptor) {
        SensorManager &sm = SensorManager::instance();
        disconnectFromSource(m_liftGestureAdaptor, "liftgesture", m_liftGestureReader);
        sm.releaseDeviceAdaptor("liftgestureadaptor");
        m_liftGestureAdaptor = nullptr;
    }

    delete m_marshallingBin;
    m_marshallingBin = nullptr;

    delete m_filterBin;
    m_filterBin = nullptr;

    delete m_outputBuffer;
    m_outputBuffer = nullptr;

    delete m_liftGestureReader;
    m_liftGestureReader = nullptr;
}

bool LiftGestureSensorChannel::start()
{
    qCDebug(lcSensorFw) << id() << "Starting LiftGestureSensorChannel";

    if (AbstractSensorChannel::start()) {
        m_marshallingBin->start();
        m_filterBin->start();
        m_liftGestureAdaptor->startSensor();
    }
    return true;
}

bool LiftGestureSensorChannel::stop()
{
    qCDebug(lcSensorFw) << id() << "Stopping LiftGestureSensorChannel";

    if (AbstractSensorChannel::stop()) {
        m_liftGestureAdaptor->stopSensor();
        m_filterBin->stop();
        m_marshallingBin->stop();
    }
    return true;
}

void LiftGestureSensorChannel::emitData(const TimedUnsigned &value)
{
    qCDebug(lcSensorFw) << id()
        << "old:" << m_previousValue.value_
        << "-->"
        << "new:" << value.value_;

    if (value.value_ != m_previousValue.value_) {
        m_previousValue.value_ = value.value_;
        //writeToClients((const void *)&value, sizeof value);
    }
    writeToClients((const void *)&value, sizeof value);
}
