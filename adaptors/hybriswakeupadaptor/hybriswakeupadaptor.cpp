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

#include <QFile>
#include <QTextStream>

#include "hybriswakeupadaptor.h"
#include "logging.h"
#include "datatypes/utils.h"
#include "config.h"

namespace {
int wakeupSensorType()
{
    QVariant setting(SensorFrameworkConfig::configuration()->value("wakeup/sensor_type"));
    bool ok = false;
    int sensorType = setting.toInt(&ok);
    if (!ok)
        sensorType = SENSOR_TYPE_WAKE_GESTURE;
    qCInfo(lcSensorFw) << "wakeupSensorType:" << sensorType;
    return sensorType;
}
}

HybrisWakeupAdaptor::HybrisWakeupAdaptor(const QString &id)
    : HybrisAdaptor(id, wakeupSensorType())
{
    m_buffer = new DeviceAdaptorRingBuffer<TimedUnsigned>(1);
    setAdaptedSensor("wakeup", "Wakeup sensor events", m_buffer);
    setDescription("Hybris wakeup");
    m_powerStatePath = SensorFrameworkConfig::configuration()->value("wakeup/powerstate_path").toByteArray();
    if (!m_powerStatePath.isEmpty() && !QFile::exists(m_powerStatePath)) {
        qCWarning(lcSensorFw) << NodeBase::id() << "Path does not exists: " << m_powerStatePath;
        m_powerStatePath.clear();
    }
}

HybrisWakeupAdaptor::~HybrisWakeupAdaptor()
{
    delete m_buffer;
}

bool HybrisWakeupAdaptor::startSensor()
{
    if (!(HybrisAdaptor::startSensor()))
        return false;
    if (isRunning() && !m_powerStatePath.isEmpty())
        writeToFile(m_powerStatePath, "1");
    qCInfo(lcSensorFw) << id() << "HybrisWakeupAdaptor start";
    return true;
}

void HybrisWakeupAdaptor::stopSensor()
{
    HybrisAdaptor::stopSensor();
    if (!isRunning() && !m_powerStatePath.isEmpty())
        writeToFile(m_powerStatePath, "0");
    qCInfo(lcSensorFw) << id() << "HybrisWakeupAdaptor stop";
}

void HybrisWakeupAdaptor::processSample(const sensors_event_t &data)
{
    TimedUnsigned *d = m_buffer->nextSlot();
    d->timestamp_ = quint64(data.timestamp * 0.001);

#ifdef USE_BINDER
    d->value_ = unsigned(data.u.scalar);
#else
    d->value_ = unsigned(data.data[0]);
#endif

    qCInfo(lcSensorFw) << id() << "HybrisWakeupAdaptor event" << d->timestamp_ << d->value_;

    m_buffer->commit();
    m_buffer->wakeUpReaders();
}
