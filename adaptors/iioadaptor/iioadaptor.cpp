/**

//    http://www.apache.org/licenses/LICENSE-2.0

   @file iioadaptor.cpp
   @brief IioAdaptor based on SysfsAdaptor

   <p>
   Copyright (C) 2009-2010 Nokia Corporation
   Copyright (C) 2012 Tuomas Kulve
   Copyright (C) 2012 Srdjan Markovic
   Copyright (C) 2016 Canonical

   @author Tuomas Kulve <tuomas@kulve.fi>
   @author Lorn Potter <lorn.potter@canonical.com>

   Sensord is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License
   version 2.1 as published by the Free Software Foundation.

   Sensord is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Sensord.  If not, see <http://www.gnu.org/licenses/>.
   </p>
*/
#include <errno.h>

#include <libudev.h>

#include <logging.h>
#include <config.h>
#include <datatypes/utils.h>
#include <unistd.h>
#include <time.h>

#include "iioadaptor.h"
#include <sysfsadaptor.h>
#include <deviceadaptorringbuffer.h>
#include <QTextStream>
#include <QDir>
#include <QTimer>
#include <QDirIterator>
#include <qmath.h>
#include <QRegularExpression>

#include <deviceadaptor.h>
#include "datatypes/orientationdata.h"

#define GRAVITY         9.80665
#define REV_GRAVITY     0.101936799

// Proximity sensor
#define PROXIMITY_DEFAULT_THRESHOLD 250
#define PROXIMITY_NEAR_VALUE 0
#define PROXIMITY_FAR_VALUE 100

/* Conversion of acceleration data to SI units (m/s^2) */
#define CONVERT_A_X(x)  ((float(x) / 1000) * (GRAVITY * -1.0))
#define CONVERT_A_Y(x)  ((float(x) / 1000) * (GRAVITY * 1.0))
#define CONVERT_A_Z(x)  ((float(x) / 1000) * (GRAVITY * 1.0))

IioAdaptor::IioAdaptor(const QString &id) :
        SysfsAdaptor(id, SysfsAdaptor::IntervalMode, true),
        deviceId(id)
{
    sensordLogD() << "Creating IioAdaptor with id:" << NodeBase::id();
    setup();
}

IioAdaptor::~IioAdaptor()
{
    if (iioXyzBuffer_)
        delete iioXyzBuffer_;
    if (alsBuffer_)
        delete alsBuffer_;
    if (magnetometerBuffer_)
        delete magnetometerBuffer_;
    if (proximityBuffer_)
        delete proximityBuffer_;
}

void IioAdaptor::setup()
{
    qDebug() << id() << Q_FUNC_INFO << deviceId;

    if (deviceId.startsWith("accel")) {
        const QString name = "accelerometer";
        const QString inputMatch = SensorFrameworkConfig::configuration()->value<QString>(name + "/input_match");
        qDebug() << id() << "input_match" << inputMatch;

        iioDevice.channelTypeName = "accel";
        devNodeNumber = findSensor(inputMatch);
        if (devNodeNumber!= -1) {
            const QString desc = "Industrial I/O accelerometer (" + iioDevice.name +")";
            qDebug() << id() << Q_FUNC_INFO << "Accelerometer found";
            iioXyzBuffer_ = new DeviceAdaptorRingBuffer<TimedXyzData>(1);
            setAdaptedSensor(name, desc, iioXyzBuffer_);

            iioDevice.sensorType = IioAdaptor::IIO_ACCELEROMETER;
        }
    } else if (deviceId.startsWith("gyro")) {
        const QString name = "gyroscope";
        const QString inputMatch = SensorFrameworkConfig::configuration()->value<QString>(name + "/input_match");
        qDebug() << id() << "input_match" << inputMatch;

        iioDevice.channelTypeName = "anglvel";
        devNodeNumber = findSensor(inputMatch);
        if (devNodeNumber!= -1) {
            const QString desc = "Industrial I/O gyroscope (" + iioDevice.name +")";
            iioXyzBuffer_ = new DeviceAdaptorRingBuffer<TimedXyzData>(1);
            setAdaptedSensor(name, desc, iioXyzBuffer_);

            iioDevice.sensorType = IioAdaptor::IIO_GYROSCOPE;
        }
    } else if (deviceId.startsWith("mag")) {
        const QString name = "magnetometer";
        const QString inputMatch = SensorFrameworkConfig::configuration()->value<QString>(name + "/input_match");
        qDebug() << id() << "input_match" << inputMatch;

        iioDevice.channelTypeName = "magn";
        devNodeNumber = findSensor(inputMatch);
        if (devNodeNumber!= -1) {
            const QString desc = "Industrial I/O magnetometer (" + iioDevice.name +")";
            magnetometerBuffer_ = new DeviceAdaptorRingBuffer<CalibratedMagneticFieldData>(1);
            setAdaptedSensor(name, desc, magnetometerBuffer_);

            iioDevice.sensorType = IioAdaptor::IIO_MAGNETOMETER;
        }
    } else if (deviceId.startsWith("als")) {
        const QString name = "als";
        const QString inputMatch = SensorFrameworkConfig::configuration()->value<QString>(name + "/input_match");

        iioDevice.channelTypeName = "illuminance";
        devNodeNumber = findSensor(inputMatch);
        if (devNodeNumber!= -1) {
            QString desc = "Industrial I/O light sensor (" + iioDevice.name +")";
            qDebug() << id() << desc;
            alsBuffer_ = new DeviceAdaptorRingBuffer<TimedUnsigned>(1);
            setAdaptedSensor(name, desc, alsBuffer_);
            iioDevice.sensorType = IioAdaptor::IIO_ALS;
        }
    } else if (deviceId.startsWith("prox")) {
        const QString name = "proximity";
        const QString inputMatch = SensorFrameworkConfig::configuration()->value<QString>(name + "/input_match");
        qDebug() << id() << name + ":" << "input_match" << inputMatch;

        iioDevice.channelTypeName = "proximity";
        devNodeNumber = findSensor(inputMatch);
        proximityThreshold = SensorFrameworkConfig::configuration()->value<QString>(name + "/threshold", QString("%1").arg(PROXIMITY_DEFAULT_THRESHOLD)).toInt();
        if (devNodeNumber!= -1) {
            QString desc = "Industrial I/O proximity sensor (" + iioDevice.name +")";
            qDebug() << id() << desc;
            proximityBuffer_ = new DeviceAdaptorRingBuffer<ProximityData>(1);
            setAdaptedSensor(name, desc, proximityBuffer_);
            iioDevice.sensorType = IioAdaptor::IIO_PROXIMITY;
        }
    }

    if (devNodeNumber == -1) {
        qDebug() << id() << Q_FUNC_INFO << "sensor is invalid";
//        setValid(false);
        return;
    }

    if (mode() != SysfsAdaptor::IntervalMode) {
        scanElementsEnable(devNodeNumber,1);
        scanElementsEnable(devNodeNumber,0);
    }

    /* Override the scaling factor if asked */
    bool ok;
    double scale_override = SensorFrameworkConfig::configuration()->value(iioDevice.name + "/scale").toDouble(&ok);
    if (ok) {
        sensordLogD() << id() << "Overriding scale to" << scale_override;
        iioDevice.scale = scale_override;
    }

    introduceAvailableDataRange(DataRange(0, 65535, 1));

    unsigned int min_interval_us =   0 * 1000;
    unsigned int max_interval_us = 586 * 1000;
    introduceAvailableInterval(DataRange(min_interval_us, max_interval_us, 0));

    unsigned int interval_us = 10 * 1000;
    setDefaultInterval(interval_us);
}

int IioAdaptor::findSensor(const QString &sensorName)
{
    udev_list_entry *devices;
    udev_list_entry *dev_list_entry;
    udev_device *dev = 0;
    struct udev *udevice = 0;
    struct udev_enumerate *enumerate = 0;

    if (!udevice)
        udevice = udev_new();

    enumerate = udev_enumerate_new(udevice);
    udev_enumerate_add_match_subsystem(enumerate, "iio");

    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    bool ok2;

    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path;
        path = udev_list_entry_get_name(dev_list_entry);

        dev = udev_device_new_from_syspath(udevice, path);
        if (qstrcmp(udev_device_get_subsystem(dev), "iio") == 0) {
            iioDevice.name = QString::fromLatin1(udev_device_get_sysattr_value(dev,"name"));
            if (iioDevice.name == sensorName) {
                struct udev_list_entry *sysattr;
                int j = 0;
                QString eventName = QString::fromLatin1(udev_device_get_sysname(dev));
                iioDevice.devicePath = QString::fromLatin1(udev_device_get_syspath(dev)) +"/";
                iioDevice.index = eventName.right(1).toInt(&ok2);
                // Default values
                iioDevice.offset = 0.0;
                iioDevice.scale = 1.0;
                iioDevice.frequency = 1.0;
                qDebug() << id() << Q_FUNC_INFO << "Syspath for sensor (" + sensorName + "):" << iioDevice.devicePath;

                udev_list_entry_foreach(sysattr, udev_device_get_sysattr_list_entry(dev)) {
                    const char *name;
                    const char *value;
                    bool ok;
                    name = udev_list_entry_get_name(sysattr);
                    value = udev_device_get_sysattr_value(dev, name);
                    if (value == NULL)
                        continue;
                    qDebug() << id() << "attr" << name << value;

                    QString attributeName(name);
                    if (attributeName.contains(QRegularExpression(iioDevice.channelTypeName + ".*scale$"))) {
                        iioDevice.scale = QString(value).toDouble(&ok);
                        if (ok) {
                            qDebug() << id() << sensorName + ":" << "Scale is" << iioDevice.scale;
                        }
                    } else if (attributeName.contains(QRegularExpression(iioDevice.channelTypeName + ".*offset$"))) {
                        iioDevice.offset = QString(value).toDouble(&ok);
                        if (ok) {
                            qDebug() << id() << sensorName + ":" << "Offset is" << value;
                        }
                    } else if (attributeName.endsWith("frequency")) {
                        iioDevice.frequency = QString(value).toDouble(&ok);
                        if (ok) {
                            qDebug() << id() << sensorName + ":" << "Frequency is" << iioDevice.frequency;
                        }
                    } else if (attributeName.contains(QRegularExpression(iioDevice.channelTypeName + ".*raw$"))) {
                        qDebug() << id() << "adding to paths:" << iioDevice.devicePath
                                   << attributeName << iioDevice.index;
                        addPath(iioDevice.devicePath + attributeName, j);
                        j++;
                    }
                }
                iioDevice.channels = j;

    // in_rot_from_north_magnetic_tilt_comp_raw ?

                // type
                break;
            }
        }
    }
    if (dev)
        udev_device_unref(dev);
    udev_enumerate_unref(enumerate);

    if (ok2)
        return iioDevice.index;
    else
        return -1;
}
/*
 * als
 * accel_3d
 * gyro_3d
 * magn_3d
 * incli_3d
 * dev_rotation
 *
 * */

bool IioAdaptor::deviceEnable(int device, int enable)
{
    qDebug() << id() << Q_FUNC_INFO <<"device"<< device <<"enable" << enable;
    qDebug() << id() << "devicePath" << iioDevice.devicePath << iioDevice.name;
    qDebug() << id() << "dev_accl_" << devNodeNumber;
    qDebug() << id() << "scale" << (double)iioDevice.scale
             << "offset" << iioDevice.offset
             << "frequency" << iioDevice.frequency;

    if (devNodeNumber == -1)
        return false;

    QString pathEnable = iioDevice.devicePath + "buffer/enable";
    QString pathLength = iioDevice.devicePath + "buffer/length";

    qDebug() << id() << pathEnable << pathLength;

    if (enable == 1) {
        // FIXME: should enable sensors for this device? Assuming enabled already
        scanElementsEnable(device, enable);
        sysfsWriteInt(pathLength, IIO_BUFFER_LEN);
        sysfsWriteInt(pathEnable, enable);
    } else {
        sysfsWriteInt(pathEnable, enable);
        scanElementsEnable(device, enable);
        // FIXME: should disable sensors for this device?
    }

    return true;
}

bool IioAdaptor::sysfsWriteInt(QString filename, int val)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        sensordLogW() << id() << "Failed to open " << filename;
        return false;
    }

    QTextStream out(&file);
    out << val << "\n";

    file.close();

	return true;
}

QString IioAdaptor::sysfsReadString(QString filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        sensordLogW() << id() << "Failed to open " << filename;
        return QString();
    }

    QTextStream in(&file);
    QString line = in.readLine();

    if (line.endsWith("\n")) {
        line.chop(1);
    }

    file.close();
    return line;
}

int IioAdaptor::sysfsReadInt(QString filename)
{
    QString string = sysfsReadString(filename);

    bool ok;
    int value = string.toInt(&ok);

    if (!ok) {
        sensordLogW() << id() << "Failed to parse '" << string << "' to int from file " << filename;
    }

	return value;
}

// Return the number of channels
int IioAdaptor::scanElementsEnable(int device, int enable)
{
    Q_UNUSED(device);

    QString elementsPath = iioDevice.devicePath + "scan_elements";

    QDir dir(elementsPath);
    if (!dir.exists()) {
        sensordLogW() << id() << "Directory " << elementsPath << " doesn't exist";
        return 0;
    }

    // Find all the *_en file and write 0/1 to it
    QStringList filters;
    filters << ("*" + iioDevice.channelTypeName + "*_en");
    dir.setNameFilters(filters);

    QFileInfoList list = dir.entryInfoList();
    for (int i = 0; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);

        if (enable) {
            QString base = fileInfo.filePath();
            // Remove the _en
            base.chop(3);

            int index = sysfsReadInt(base + "_index");
            int bytes = deviceChannelParseBytes(base + "_type");

           iioDevice.channel_bytes[index] = bytes;
        }

        sysfsWriteInt(fileInfo.filePath(), enable);
    }

    return list.size();
}

int IioAdaptor::deviceChannelParseBytes(QString filename)
{
    QString type = sysfsReadString(filename);

    if (type.compare("le:s16/16>>0") == 0) {
        return 2;
    } else if (type.compare("le:s32/32>>0") == 0) {
        return 4;
    } else if (type.compare("le:s64/64>>0") == 0) {
        return 8;
    } else {
        sensordLogW() << id() << "ERROR: invalid type from file " << filename << ": " << type;
    }

    return 0;
}

void IioAdaptor::processSample(int fileId, int fd)
{
    char buf[IIO_BUFFER_LEN];
    int readBytes = 0;
    qreal result = 0;
    int channel = fileId%IIO_MAX_DEVICE_CHANNELS;
    int device = (fileId - channel)/IIO_MAX_DEVICE_CHANNELS;

    if (device == 0) {
        readBytes = read(fd, buf, sizeof(buf));

        if (readBytes <= 0) {
            sensordLogW() << id() << "read():" << strerror(errno);
            return;
        }

        errno = 0; // reset errno before call
        result = strtol(buf, NULL, 10);
        
        // If any conversion error occurs, abort
        if (errno != 0) {
            sensordLogW() << id() << "strtol(): Unable to convert string to long";
            return;
        }

        switch(channel) {
        case 0: {
            switch (iioDevice.sensorType) {
            case IioAdaptor::IIO_ACCELEROMETER:
            case IioAdaptor::IIO_GYROSCOPE:
                timedData = iioXyzBuffer_->nextSlot();
                timedData->x_= -(result + iioDevice.offset) * iioDevice.scale * 1000 * REV_GRAVITY;
                break;
            case IioAdaptor::IIO_MAGNETOMETER:
                calData = magnetometerBuffer_->nextSlot();
                calData->rx_ = (result + iioDevice.offset) * iioDevice.scale;
                break;
            case IioAdaptor::IIO_ALS:
                uData = alsBuffer_->nextSlot();
                uData->value_ = (result + iioDevice.offset) * iioDevice.scale;
                break;
            case IioAdaptor::IIO_PROXIMITY:
                {
                    bool near = false;
                    int proximityValue = (result + iioDevice.offset) * iioDevice.scale;
                    proximityData = proximityBuffer_->nextSlot();
                    // IIO proximity sensors are inverted in comparison to Hybris proximity sensors
                    if (proximityValue >= proximityThreshold) {
                        near = true;
                    }
                    proximityData->withinProximity_ = near;
                    proximityData->value_ = near ? PROXIMITY_NEAR_VALUE : PROXIMITY_FAR_VALUE;
                }
                break;
            default:
                break;
            };
        }
            break;

        case 1: {
            switch (iioDevice.sensorType) {
            case IioAdaptor::IIO_ACCELEROMETER:
            case IioAdaptor::IIO_GYROSCOPE:
                timedData = iioXyzBuffer_->nextSlot();
                timedData->y_= -(result + iioDevice.offset) * iioDevice.scale * 1000 * REV_GRAVITY;
                break;
            case IioAdaptor::IIO_MAGNETOMETER:
                calData = magnetometerBuffer_->nextSlot();
                result = (result * iioDevice.scale);
                calData->y_ = result;
                break;
            default:
                break;
            };
        }
            break;

        case 2: {
            switch (iioDevice.sensorType) {
            case IioAdaptor::IIO_ACCELEROMETER:
            case IioAdaptor::IIO_GYROSCOPE:
                timedData = iioXyzBuffer_->nextSlot();
                timedData->z_ = -(result + iioDevice.offset) * iioDevice.scale * 1000 * REV_GRAVITY;
                break;
            case IioAdaptor::IIO_MAGNETOMETER:
                calData = magnetometerBuffer_->nextSlot();
                result = ((result + iioDevice.offset) * iioDevice.scale) * 100;
                calData->rz_ = result;
                break;
            default:
                break;
            };
        }
            break;
        };

        if (channel == iioDevice.channels - 1) {
            switch (iioDevice.sensorType) {
            case IioAdaptor::IIO_ACCELEROMETER:
            case IioAdaptor::IIO_GYROSCOPE:
                timedData->timestamp_ = Utils::getTimeStamp();
                iioXyzBuffer_->commit();
                iioXyzBuffer_->wakeUpReaders();
                break;
            case IioAdaptor::IIO_MAGNETOMETER:
                calData->timestamp_ = Utils::getTimeStamp();
                magnetometerBuffer_->commit();
                magnetometerBuffer_->wakeUpReaders();
                break;
            case IioAdaptor::IIO_ALS:
                uData->timestamp_ = Utils::getTimeStamp();
                alsBuffer_->commit();
                alsBuffer_->wakeUpReaders();
                sensordLogT() << id() << "ALS offset=" << iioDevice.offset << "scale=" << iioDevice.scale << "value=" << uData->value_ << "timestamp=" << uData->timestamp_;
                break;
            case IioAdaptor::IIO_PROXIMITY:
                proximityData->timestamp_ = Utils::getTimeStamp();
                proximityBuffer_->commit();
                proximityBuffer_->wakeUpReaders();
                sensordLogT() << id() << "Proximity offset=" << iioDevice.offset << "scale=" << iioDevice.scale << "value=" << proximityData->value_ << "within proximity=" << proximityData->withinProximity_ << "timestamp=" << proximityData->timestamp_;
                break;
            default:
                break;
            };
        }
    }
}

bool IioAdaptor::setInterval(const int sessionId, const unsigned int interval_us)
{
    if (mode() == SysfsAdaptor::IntervalMode)
        return SysfsAdaptor::setInterval(sessionId, interval_us);

    sensordLogD() << id() << "Ignoring setInterval for " << interval_us;

    return true;
}

//unsigned int IioAdaptor::interval() const
//{
//    int interval_us = 100 * 1000;
//    sensordLogD() << id() << "Returning dummy value in interval(): " << interval_us;
//    return interval_us;
//}


bool IioAdaptor::startSensor()
{
    if (devNodeNumber == -1)
        return false;

    qDebug() << id() << Q_FUNC_INFO;
    if (mode() != SysfsAdaptor::IntervalMode)
        deviceEnable(devNodeNumber, true);
    return SysfsAdaptor::startSensor();
}

void IioAdaptor::stopSensor()
{
    if (devNodeNumber == -1)
        return;
    qDebug() << id() << Q_FUNC_INFO;
    if (mode() != SysfsAdaptor::IntervalMode)
        deviceEnable(devNodeNumber, false);
    SysfsAdaptor::stopSensor();
}
