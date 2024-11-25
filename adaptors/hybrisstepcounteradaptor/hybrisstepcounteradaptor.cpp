/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd
**
** Copyright (C) 2016 kimmoli
**
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

#include "hybrisstepcounteradaptor.h"
#include "logging.h"
#include "datatypes/utils.h"
#include "config.h"

#ifndef USE_BINDER
#include <android-version.h>
#endif

HybrisStepCounterAdaptor::HybrisStepCounterAdaptor(const QString& id) :
    HybrisAdaptor(id, SENSOR_TYPE_STEP_COUNTER)
{
    buffer = new DeviceAdaptorRingBuffer<TimedUnsigned>(1);
    setAdaptedSensor("stepcounter", "Internal step counter steps since reboot", buffer);
    setDescription("Hybris step counter");
    powerStatePath = SensorFrameworkConfig::configuration()->value("stepcounter/powerstate_path").toByteArray();
    if (!powerStatePath.isEmpty() && !QFile::exists(powerStatePath))
    {
        qCWarning(lcSensorFw) << NodeBase::id() << "Path does not exists: " << powerStatePath;
    	powerStatePath.clear();
    }
}

HybrisStepCounterAdaptor::~HybrisStepCounterAdaptor()
{
    delete buffer;
}

bool HybrisStepCounterAdaptor::startSensor()
{
    if (!(HybrisAdaptor::startSensor()))
        return false;
    if (isRunning() && !powerStatePath.isEmpty())
        writeToFile(powerStatePath, "1");
    qCInfo(lcSensorFw) << id() << "Hybris HybrisStepCounterAdaptor start";
    return true;
}

void HybrisStepCounterAdaptor::sendInitialData()
{
    qCWarning(lcSensorFw) << id() << "No initial data for step counter";
}

void HybrisStepCounterAdaptor::stopSensor()
{
    HybrisAdaptor::stopSensor();
    if (!isRunning() && !powerStatePath.isEmpty())
        writeToFile(powerStatePath, "0");
    qCInfo(lcSensorFw) << id() << "Hybris HybrisStepCounterAdaptor stop";
}

void HybrisStepCounterAdaptor::processSample(const sensors_event_t& data)
{
    TimedUnsigned *d = buffer->nextSlot();
    d->timestamp_ = quint64(data.timestamp * .001);
#ifdef USE_BINDER
    d->value_ = data.u.stepCount;
#else
#if ANDROID_VERSION_MAJOR == 4 && ANDROID_VERSION_MINOR < 4
/* In Android versions 4.3 and older hardware/sensors.h does not
 * contain u64 union and values are located at the same
 * address as non-u64 data array. */
    uint64_t value = 0;
    memcpy(&value, data.data, sizeof value);
    d->value_ = value;
#else
    d->value_ = data.u64.step_counter;
#endif
#endif
    buffer->commit();
    buffer->wakeUpReaders();
}
