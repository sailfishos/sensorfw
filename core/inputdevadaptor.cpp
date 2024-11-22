/**
   @file inputdevadaptor.cpp
   @brief Base class for input layer device adaptors

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Matias Muhonen <ext-matias.muhonen@nokia.com>
   @author Antti Virtanen <antti.i.virtanen@nokia.com>
   @author Lihan Guo <ext-lihan.4.guo@nokia.com>
   @author Shenghua <ext-shenghua.1.liu@nokia.com>
   @author Antti Virtanen <antti.i.virtanen@nokia.com>

   This file is part of Sensord.

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

#include "inputdevadaptor.h"
#include "config.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <QFile>
#include <QDir>
#include <QString>

InputDevAdaptor::InputDevAdaptor(const QString& id, int maxDeviceCount) :
    SysfsAdaptor(id, SysfsAdaptor::SelectMode, false),
    m_deviceCount(0),
    m_maxDeviceCount(maxDeviceCount),
    m_cachedInterval_us(0)
{
    memset(m_evlist, 0x0, sizeof(input_event)*64);
}

InputDevAdaptor::~InputDevAdaptor()
{
}

int InputDevAdaptor::getInputDevices(const QString& typeName)
{
    qDebug() << id() << Q_FUNC_INFO << typeName;
    QString deviceSysPathString = SensorFrameworkConfig::configuration()->value("global/device_sys_path").toString();
    QString devicePollFilePath = SensorFrameworkConfig::configuration()->value("global/device_poll_file_path").toString();

    int deviceNumber = 0;
    m_deviceString = typeName;

    // Check if this device name is defined in configuration
    QString deviceName = SensorFrameworkConfig::configuration()->value<QString>(typeName + "/device", "");

    // Do not perform strict checks for the input device
    if (deviceName.size() && checkInputDevice(deviceName, typeName, false)) {
        addPath(deviceName, m_deviceCount);
        ++m_deviceCount;
    } else if (deviceSysPathString.contains("%1")) {
        const int MAX_EVENT_DEV = 16;
        qDebug() << id() << deviceNumber << m_deviceCount << m_maxDeviceCount;

        // No configuration for this device, try find the device from the device system path
        while (deviceNumber < MAX_EVENT_DEV && m_deviceCount < m_maxDeviceCount) {
            deviceName = deviceSysPathString.arg(deviceNumber);
            qDebug() << id() << Q_FUNC_INFO << deviceName;
            if (checkInputDevice(deviceName, typeName)) {
                addPath(deviceName, m_deviceCount);
                ++m_deviceCount;
                break;
            }
            ++deviceNumber;
        }
    }

    QString pollConfigKey = QString(typeName + "/poll_file");
    if (SensorFrameworkConfig::configuration()->exists(pollConfigKey)) {
        m_usedDevicePollFilePath = SensorFrameworkConfig::configuration()->value<QString>(pollConfigKey, "");
    } else {
        m_usedDevicePollFilePath = devicePollFilePath.arg(deviceNumber);
    }
    qDebug() << id() << Q_FUNC_INFO << m_usedDevicePollFilePath;

    if (m_deviceCount == 0) {
        sensordLogW() << id() << "Cannot find any device for: " << typeName;
        setValid(false);
    } else {
        QByteArray byteArray = readFromFile(m_usedDevicePollFilePath.toLatin1());
        int interval_ms = byteArray.size() > 0 ? byteArray.toInt() : 0;
        m_cachedInterval_us = interval_ms * 1000;
    }

    return m_deviceCount;
}

int InputDevAdaptor::getEvents(int fd)
{
    int bytes = read(fd, m_evlist, sizeof(struct input_event)*64);
    if (bytes == -1) {
        sensordLogW() << id() << "Error occured: " << strerror(errno);
        return 0;
    }
    if (bytes % sizeof(struct input_event)) {
        sensordLogW() << id() << "Short read or stray bytes.";
        return 0;
    }
    return bytes/sizeof(struct input_event);
}

void InputDevAdaptor::processSample(int pathId, int fd)
{
    int numEvents = getEvents(fd);

    for (int i = 0; i < numEvents; ++i) {
        switch (m_evlist[i].type) {
            case EV_SYN:
                interpretSync(pathId, &(m_evlist[i]));
                break;
            default:
                interpretEvent(pathId, &(m_evlist[i]));
                break;
        }
    }
}

bool InputDevAdaptor::checkInputDevice(const QString& path, const QString& matchString, bool strictChecks) const
{
    char deviceName[256] = {0,};
    bool check = true;
    qDebug() << id() << Q_FUNC_INFO << path << matchString << strictChecks;
    int fd = open(path.toLocal8Bit().constData(), O_RDONLY);
    if (fd == -1) {
        return false;
    }

    if (strictChecks) {
        int result = ioctl(fd, EVIOCGNAME(sizeof(deviceName)), deviceName);
        qDebug() << id() << Q_FUNC_INFO << "open result:" << result << deviceName;

        if (result == -1) {
           sensordLogW() << id() << "Could not read devicename for " << path;
           check = false;
        } else {
            if (QString(deviceName).contains(matchString, Qt::CaseInsensitive)) {
                sensordLogT() << id() << "\"" << matchString << "\"" << " matched in device name: " << deviceName;
                check = true;
            } else {
                check = false;
            }
        }
    }

    close(fd);

    return check;
}

unsigned int InputDevAdaptor::interval() const
{
    return m_cachedInterval_us;
}

bool InputDevAdaptor::setInterval(const int sessionId, const unsigned int interval_us)
{
    Q_UNUSED(sessionId);

    // XXX: is this supposed to be sampling frequency or sample time?
    int interval_ms = (interval_us + 999) / 1000;
    sensordLogD() << id() << "Setting poll interval for " << m_deviceString << " to " << interval_ms << "ms";
    QByteArray frequencyString(QString("%1\n").arg(interval_ms).toLocal8Bit());
    if (writeToFile(m_usedDevicePollFilePath.toLocal8Bit(), frequencyString)) {
        m_cachedInterval_us = interval_ms * 1000;
        return true;
    }
    return false;
}

void InputDevAdaptor::init()
{
    qDebug() << id() << Q_FUNC_INFO << name();
    if (!getInputDevices(SensorFrameworkConfig::configuration()->value<QString>(name() + "/input_match", name()))) {
        sensordLogW() << id() << "Input device not found.";
        SysfsAdaptor::init();
    }
}

int InputDevAdaptor::getDeviceCount() const
{
    return m_deviceCount;
}
