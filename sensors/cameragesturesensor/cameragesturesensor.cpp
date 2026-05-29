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

#include "cameragesturesensor.h"

#include "sensormanager.h"
#include "bin.h"
#include "bufferreader.h"

CameraGestureSensorChannel::CameraGestureSensorChannel(const QString &id)
    : AbstractSensorChannel(id)
    , DataEmitter<TimedUnsigned>(1)
    , m_previousValue(0, 0)
    , m_filterBin(nullptr)
    , m_marshallingBin(nullptr)
    , m_cameraGestureAdaptor(nullptr)
    , m_cameraGestureReader(nullptr)
    , m_outputBuffer(nullptr)
{
    SensorManager &sm = SensorManager::instance();

    m_cameraGestureAdaptor = sm.requestDeviceAdaptor("cameragestureadaptor");
    if (!m_cameraGestureAdaptor) {
        setValid(false);
        return;
    }

    m_cameraGestureReader = new BufferReader<TimedUnsigned>(1);

    m_outputBuffer = new RingBuffer<TimedUnsigned>(1);

    // Create buffers for filter chain
    m_filterBin = new Bin;

    m_filterBin->add(m_cameraGestureReader, "cameragesture");
    m_filterBin->add(m_outputBuffer, "buffer");

    m_filterBin->join("cameragesture", "source", "buffer", "sink");

    // Join datasources to the chain
    connectToSource(m_cameraGestureAdaptor, "cameragesture", m_cameraGestureReader);

    m_marshallingBin = new Bin;
    m_marshallingBin->add(this, "sensorchannel");

    m_outputBuffer->join(this);

    setDescription("cameragesture events");
    setRangeSource(m_cameraGestureAdaptor);
    addStandbyOverrideSource(m_cameraGestureAdaptor);
    setIntervalSource(m_cameraGestureAdaptor);

    setValid(true);
}

CameraGestureSensorChannel::~CameraGestureSensorChannel()
{
    if (m_cameraGestureAdaptor) {
        SensorManager &sm = SensorManager::instance();
        disconnectFromSource(m_cameraGestureAdaptor, "cameragesture", m_cameraGestureReader);
        sm.releaseDeviceAdaptor("cameragestureadaptor");
        m_cameraGestureAdaptor = nullptr;
    }

    delete m_marshallingBin;
    m_marshallingBin = nullptr;

    delete m_filterBin;
    m_filterBin = nullptr;

    delete m_outputBuffer;
    m_outputBuffer = nullptr;

    delete m_cameraGestureReader;
    m_cameraGestureReader = nullptr;
}

bool CameraGestureSensorChannel::start()
{
    qCDebug(lcSensorFw) << id() << "Starting CameraGestureSensorChannel";

    if (AbstractSensorChannel::start()) {
        m_marshallingBin->start();
        m_filterBin->start();
        m_cameraGestureAdaptor->startSensor();
    }
    return true;
}

bool CameraGestureSensorChannel::stop()
{
    qCDebug(lcSensorFw) << id() << "Stopping CameraGestureSensorChannel";

    if (AbstractSensorChannel::stop()) {
        m_cameraGestureAdaptor->stopSensor();
        m_filterBin->stop();
        m_marshallingBin->stop();
    }
    return true;
}

void CameraGestureSensorChannel::emitData(const TimedUnsigned &value)
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
