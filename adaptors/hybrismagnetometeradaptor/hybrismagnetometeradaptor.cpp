/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd
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

#include "hybrismagnetometeradaptor.h"
#include "logging.h"
#include "datatypes/utils.h"
#include "config.h"

HybrisMagnetometerAdaptor::HybrisMagnetometerAdaptor(const QString& id) :
    HybrisAdaptor(id,SENSOR_TYPE_MAGNETIC_FIELD)
{
    buffer = new DeviceAdaptorRingBuffer<CalibratedMagneticFieldData>(1);
    setAdaptedSensor("magnetometer", "Internal magnetometer coordinates", buffer);

    setDescription("Hybris magnetometer");
    powerStatePath = SensorFrameworkConfig::configuration()->value("magnetometer/powerstate_path").toByteArray();
    if (!powerStatePath.isEmpty() && !QFile::exists(powerStatePath)) {
        sensordLogW() << NodeBase::id() << "Path does not exists: " << powerStatePath;
    	powerStatePath.clear();
    }
    //setStandbyOverride(false);
    unsigned int interval_us = 50 * 1000;
    setDefaultInterval(interval_us);
}

HybrisMagnetometerAdaptor::~HybrisMagnetometerAdaptor()
{
    delete buffer;
}

bool HybrisMagnetometerAdaptor::startSensor()
{
    if (!(HybrisAdaptor::startSensor()))
        return false;
    if (isRunning() &&!powerStatePath.isEmpty())
        writeToFile(powerStatePath, "1");
    sensordLogD() << id() << "HybrisMagnetometerAdaptor start";
    return true;
}

void HybrisMagnetometerAdaptor::stopSensor()
{
    HybrisAdaptor::stopSensor();
    if (!isRunning() && !powerStatePath.isEmpty())
        writeToFile(powerStatePath, "0");
    sensordLogD() << id() << "HybrisMagnetometerAdaptor stop";
}

void HybrisMagnetometerAdaptor::processSample(const sensors_event_t& data)
{
    CalibratedMagneticFieldData *d = buffer->nextSlot();
    d->timestamp_ = quint64(data.timestamp * .001);

#ifdef USE_BINDER
    d->x_ = data.u.vec3.x * 1000;
    d->y_ = data.u.vec3.y * 1000;
    d->z_ = data.u.vec3.z * 1000;
    d->rx_ = data.u.vec3.x * 1000;
    d->ry_ = data.u.vec3.y * 1000;
    d->rz_ = data.u.vec3.z * 1000;
    d->level_= data.u.vec3.status;
#else
    //uT
    d->x_ = (data.magnetic.x * 1000);
    d->y_ = (data.magnetic.y * 1000);
    d->z_ = (data.magnetic.z * 1000);
    d->level_= data.magnetic.status;
#ifdef SENSORS_DEVICE_API_VERSION_1_1
    d->rx_ = data.uncalibrated_magnetic.x_uncalib * 1000;
    d->ry_ = data.uncalibrated_magnetic.y_uncalib * 1000;
    d->rz_ = data.uncalibrated_magnetic.z_uncalib * 1000;
#else
    d->rx_ = data.magnetic.x * 1000;
    d->ry_ = data.magnetic.y * 1000;
    d->rz_ = data.magnetic.z * 1000;
#endif
#endif
    buffer->commit();
    buffer->wakeUpReaders();
}
