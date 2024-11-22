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

#include <QFile>
#include <QTextStream>

#include "hybrisalsadaptor.h"
#include "logging.h"
#include "datatypes/utils.h"
#include "config.h"

#include <fcntl.h>
#include <unistd.h>

HybrisAlsAdaptor::HybrisAlsAdaptor(const QString& id) :
    HybrisAdaptor(id,SENSOR_TYPE_LIGHT),
    lastLightValue(9999)
{
    buffer = new DeviceAdaptorRingBuffer<TimedUnsigned>(1);
    setAdaptedSensor("als", "Internal ambient light sensor lux values", buffer);
    setDescription("Hybris als");
    powerStatePath = SensorFrameworkConfig::configuration()->value("als/powerstate_path").toByteArray();
    if (!powerStatePath.isEmpty() && !QFile::exists(powerStatePath)) {
        sensordLogW() << NodeBase::id() << "Path does not exists: " << powerStatePath;
    	powerStatePath.clear();
    }
}

HybrisAlsAdaptor::~HybrisAlsAdaptor()
{
    delete buffer;
}

bool HybrisAlsAdaptor::startSensor()
{
    if (!(HybrisAdaptor::startSensor()))
        return false;
    if (isRunning() && !powerStatePath.isEmpty())
        writeToFile(powerStatePath, "1");
    sensordLogD() << id() << "Hybris HybrisAlsAdaptor start";
    return true;
}

void HybrisAlsAdaptor::sendInitialData()
{
    QFile file("/proc/bus/input/devices");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        bool ok = false;
        QString inputDev;

        QTextStream in(&file);
        QString line = in.readLine();
        while (!line.isNull()) {
            if (ok && line.startsWith("S: Sysfs=")) {
                inputDev = line.split("=").at(1).section("/",-1);
                ok = false;
                break;
            }
            if (line.contains("als")) {
                ok = true;
            }
            line = in.readLine();
        }

        if (inputDev.isEmpty()) {
            sensordLogD() << id() << "No sysfs als device found";
            return;
        }

        struct input_absinfo absinfo;
        int fd;
        inputDev.replace("input","event");
        inputDev.prepend("/dev/input/");

        if ((fd = open(inputDev.toLatin1(), O_RDONLY)) > -1) {

            if (!ioctl(fd, EVIOCGABS(ABS_MISC), &absinfo)) {
                if (absinfo.value != (signed)lastLightValue)
                    lastLightValue = absinfo.value;

                TimedUnsigned *d = buffer->nextSlot();
                d->timestamp_ = Utils::getTimeStamp();
                d->value_ = lastLightValue;
                buffer->commit();
                buffer->wakeUpReaders();
            } else {
                qDebug() << id() << "ioctl not opened" ;
            }
            close(fd);
        } else {
            qDebug() << id() << "could not open als evdev";
            TimedUnsigned *d = buffer->nextSlot();
            d->timestamp_ = Utils::getTimeStamp();
            d->value_ = lastLightValue;
            buffer->commit();
            buffer->wakeUpReaders();
        }
    }
}

void HybrisAlsAdaptor::stopSensor()
{
    HybrisAdaptor::stopSensor();
    if (!isRunning() && !powerStatePath.isEmpty())
        writeToFile(powerStatePath, "0");
    sensordLogD() << id() << "Hybris HybrisAlsAdaptor stop";
}

void HybrisAlsAdaptor::processSample(const sensors_event_t& data)
{
    TimedUnsigned *d = buffer->nextSlot();
    d->timestamp_ = quint64(data.timestamp * .001);
#ifdef USE_BINDER
    d->value_ = data.u.scalar;
#else
    d->value_ = data.light;
#endif
    lastLightValue = d->value_;
    buffer->commit();
    buffer->wakeUpReaders();
}
