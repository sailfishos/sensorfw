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

#include "hybrisorientationadaptor.h"
#include "logging.h"
#include "datatypes/utils.h"
#include "config.h"

/*
 * azimuth: angle between the magnetic north direction and the Y axis, around
 * the Z axis (0<=azimuth<360).
 * 0=North, 90=East, 180=South, 270=West
 *
 * pitch: Rotation around X axis (-180<=pitch<=180), with positive values when
 * the z-axis moves toward the y-axis.
 *
 * roll: Rotation around Y axis (-90<=roll<=90), with positive values when
 * the x-axis moves towards the z-axis.
 *
 **/

/*
 * not to be confused with orientation
 **/

HybrisOrientationAdaptor::HybrisOrientationAdaptor(const QString& id) :
    HybrisAdaptor(id,SENSOR_TYPE_ORIENTATION)
{
    buffer = new DeviceAdaptorRingBuffer<CompassData>(1);
    setAdaptedSensor("hybrisorientation", "Internal orientation coordinates", buffer);

    setDescription("Hybris orientation");
    powerStatePath = SensorFrameworkConfig::configuration()->value("orientation/powerstate_path").toByteArray();
    if (!powerStatePath.isEmpty() && !QFile::exists(powerStatePath)) {
        sensordLogW() << NodeBase::id() << "Path does not exists: " << powerStatePath;
    	powerStatePath.clear();
    }
//    unsigned int interval_us = 50 * 1000;
//    setDefaultInterval(interval_us);
}

HybrisOrientationAdaptor::~HybrisOrientationAdaptor()
{
    delete buffer;
}

bool HybrisOrientationAdaptor::startSensor()
{
    if (!(HybrisAdaptor::startSensor()))
        return false;
    if (isRunning() && !powerStatePath.isEmpty())
        writeToFile(powerStatePath, "1");
    sensordLogD() << id() << "Hybris OrientationAdaptor start";
    return true;
}

void HybrisOrientationAdaptor::stopSensor()
{
    HybrisAdaptor::stopSensor();
    if (!isRunning() && !powerStatePath.isEmpty())
        writeToFile(powerStatePath, "0");
    sensordLogD() << id() << "Hybris OrientationAdaptor stop";
}

void HybrisOrientationAdaptor::processSample(const sensors_event_t& data)
{
    CompassData *d = buffer->nextSlot();
    d->timestamp_ = quint64(data.timestamp * .001);
#ifdef USE_BINDER
    d->degrees_ = data.u.vec3.x; //azimuth
    d->level_ = data.u.vec3.status;
#else
    d->degrees_ = data.orientation.azimuth; //azimuth
    d->level_ = data.orientation.status;
#endif
    d->rawDegrees_ = d->degrees_;

    buffer->commit();
    buffer->wakeUpReaders();
}
