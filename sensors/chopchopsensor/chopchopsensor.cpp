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

#include "chopchopsensor.h"

#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"

ChopChopSensorChannel::ChopChopSensorChannel(const QString &id)
    : AbstractSensorChannel(id)
    , DataEmitter<TimedUnsigned>(1)
    , m_previousValue(0, 0)
    , m_filterBin(nullptr)
    , m_marshallingBin(nullptr)
    , m_chopChopAdaptor(nullptr)
    , m_chopChopReader(nullptr)
    , m_outputBuffer(nullptr)
{
    SensorManager &sm = SensorManager::instance();

    m_chopChopAdaptor = sm.requestDeviceAdaptor("chopchopadaptor");
    if (!m_chopChopAdaptor) {
        setValid(false);
        return;
    }

    m_chopChopReader = new BufferReader<TimedUnsigned>(1);

    m_outputBuffer = new RingBuffer<TimedUnsigned>(1);

    // Create buffers for filter chain
    m_filterBin = new Bin;

    m_filterBin->add(m_chopChopReader, "chopchop");
    m_filterBin->add(m_outputBuffer, "buffer");

    m_filterBin->join("chopchop", "source", "buffer", "sink");

    // Join datasources to the chain
    connectToSource(m_chopChopAdaptor, "chopchop", m_chopChopReader);

    m_marshallingBin = new Bin;
    m_marshallingBin->add(this, "sensorchannel");

    m_outputBuffer->join(this);

    setDescription("chopchop events");
    setRangeSource(m_chopChopAdaptor);
    addStandbyOverrideSource(m_chopChopAdaptor);
    setIntervalSource(m_chopChopAdaptor);

    setValid(true);
}

ChopChopSensorChannel::~ChopChopSensorChannel()
{
    if (m_chopChopAdaptor) {
        SensorManager &sm = SensorManager::instance();
        disconnectFromSource(m_chopChopAdaptor, "chopchop", m_chopChopReader);
        sm.releaseDeviceAdaptor("chopchopadaptor");
        m_chopChopAdaptor = nullptr;
    }

    delete m_marshallingBin;
    m_marshallingBin = nullptr;

    delete m_filterBin;
    m_filterBin = nullptr;

    delete m_outputBuffer;
    m_outputBuffer = nullptr;

    delete m_chopChopReader;
    m_chopChopReader = nullptr;
}

bool ChopChopSensorChannel::start()
{
    qCDebug(lcSensorFw) << id() << "Starting ChopChopSensorChannel";

    if (AbstractSensorChannel::start()) {
        m_marshallingBin->start();
        m_filterBin->start();
        m_chopChopAdaptor->startSensor();
    }
    return true;
}

bool ChopChopSensorChannel::stop()
{
    qCDebug(lcSensorFw) << id() << "Stopping ChopChopSensorChannel";

    if (AbstractSensorChannel::stop()) {
        m_chopChopAdaptor->stopSensor();
        m_filterBin->stop();
        m_marshallingBin->stop();
    }
    return true;
}

void ChopChopSensorChannel::emitData(const TimedUnsigned &value)
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
