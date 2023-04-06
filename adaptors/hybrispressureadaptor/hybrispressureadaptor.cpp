/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd
**
** Copyright (C) 2017 Matti Lehtimäki
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

#include "hybrispressureadaptor.h"
#include "logging.h"
#include "datatypes/utils.h"
#include "config.h"

HybrisPressureAdaptor::HybrisPressureAdaptor(const QString& id) :
    HybrisAdaptor(id,SENSOR_TYPE_PRESSURE)
{
    buffer = new DeviceAdaptorRingBuffer<TimedUnsigned>(1);
    setAdaptedSensor("pressure", "Internal ambient pressure sensor values", buffer);
    setDescription("Hybris pressure");
    powerStatePath = SensorFrameworkConfig::configuration()->value("pressure/powerstate_path").toByteArray();
    if (!powerStatePath.isEmpty() && !QFile::exists(powerStatePath))
    {
        sensordLogW() << NodeBase::id() << "Path does not exists: " << powerStatePath;
        powerStatePath.clear();
    }
}

HybrisPressureAdaptor::~HybrisPressureAdaptor()
{
    delete buffer;
}

bool HybrisPressureAdaptor::startSensor()
{
    if (!(HybrisAdaptor::startSensor()))
        return false;
    if (isRunning() && !powerStatePath.isEmpty())
        writeToFile(powerStatePath, "1");
    sensordLogD() << id() << "Hybris HybrisPressureAdaptor start";
    return true;
}

void HybrisPressureAdaptor::stopSensor()
{
    HybrisAdaptor::stopSensor();
    if (!isRunning() && !powerStatePath.isEmpty())
        writeToFile(powerStatePath, "0");
    sensordLogD() << id() << "Hybris HybrisPressureAdaptor stop";
}

void HybrisPressureAdaptor::processSample(const sensors_event_t& data)
{
    TimedUnsigned *d = buffer->nextSlot();
    d->timestamp_ = quint64(data.timestamp * .001);
#ifdef USE_BINDER
    d->value_ = data.u.scalar * 100;//From hPa to Pa
#else
    d->value_ = data.pressure * 100;//From hPa to Pa
#endif
    buffer->commit();
    buffer->wakeUpReaders();
}
