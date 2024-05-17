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

#include "wakeupsensor.h"

#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"

WakeupSensorChannel::WakeupSensorChannel(const QString &id)
    : AbstractSensorChannel(id)
    , DataEmitter<TimedUnsigned>(1)
    , m_previousValue(0, 0)
    , m_filterBin(nullptr)
    , m_marshallingBin(nullptr)
    , m_wakeupAdaptor(nullptr)
    , m_wakeupReader(nullptr)
    , m_outputBuffer(nullptr)
{
    SensorManager &sm = SensorManager::instance();

    m_wakeupAdaptor = sm.requestDeviceAdaptor("wakeupadaptor");
    if (!m_wakeupAdaptor) {
        setValid(false);
        return;
    }

    m_wakeupReader = new BufferReader<TimedUnsigned>(1);

    m_outputBuffer = new RingBuffer<TimedUnsigned>(1);

    // Create buffers for filter chain
    m_filterBin = new Bin;

    m_filterBin->add(m_wakeupReader, "wakeup");
    m_filterBin->add(m_outputBuffer, "buffer");

    m_filterBin->join("wakeup", "source", "buffer", "sink");

    // Join datasources to the chain
    connectToSource(m_wakeupAdaptor, "wakeup", m_wakeupReader);

    m_marshallingBin = new Bin;
    m_marshallingBin->add(this, "sensorchannel");

    m_outputBuffer->join(this);

    setDescription("wakeup events");
    setRangeSource(m_wakeupAdaptor);
    addStandbyOverrideSource(m_wakeupAdaptor);
    setIntervalSource(m_wakeupAdaptor);

    setValid(true);
}

WakeupSensorChannel::~WakeupSensorChannel()
{
    if (m_wakeupAdaptor) {
        SensorManager &sm = SensorManager::instance();
        disconnectFromSource(m_wakeupAdaptor, "wakeup", m_wakeupReader);
        sm.releaseDeviceAdaptor("wakeupadaptor");
        m_wakeupAdaptor = nullptr;
    }

    delete m_marshallingBin;
    m_marshallingBin = nullptr;

    delete m_filterBin;
    m_filterBin = nullptr;

    delete m_outputBuffer;
    m_outputBuffer = nullptr;

    delete m_wakeupReader;
    m_wakeupReader = nullptr;
}

bool WakeupSensorChannel::start()
{
    qCDebug(lcSensorFw) << id() << "Starting WakeupSensorChannel";

    if (AbstractSensorChannel::start()) {
        m_marshallingBin->start();
        m_filterBin->start();
        m_wakeupAdaptor->startSensor();
    }
    return true;
}

bool WakeupSensorChannel::stop()
{
    qCDebug(lcSensorFw) << id() << "Stopping WakeupSensorChannel";

    if (AbstractSensorChannel::stop()) {
        m_wakeupAdaptor->stopSensor();
        m_filterBin->stop();
        m_marshallingBin->stop();
    }
    return true;
}

void WakeupSensorChannel::emitData(const TimedUnsigned &value)
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
