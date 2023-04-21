/**
   @file sysfsadaptor.cpp
   @brief Base class for device adaptors reading data from files

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Antti Virtanen <antti.i.virtanen@nokia.com>
   @author Shenghua <ext-shenghua.1.liu@nokia.com>

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

#include "sysfsadaptor.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <QFile>
#include "logging.h"
#include "config.h"

SysfsAdaptor::SysfsAdaptor(const QString& id,
                           PollMode mode,
                           bool seek,
                           const QString& path,
                           const int pathId) :
    DeviceAdaptor(id),
    m_reader(this),
    m_mode(mode),
    m_epollDescriptor(-1),
    m_interval_us(0),
    m_inStandbyMode(false),
    m_running(false),
    m_shouldBeRunning(false),
    m_doSeek(seek)
{
    if (!path.isEmpty()) {
        addPath(path, pathId);
    }

    m_pipeDescriptors[0] = -1;
    m_pipeDescriptors[1] = -1;
}

SysfsAdaptor::~SysfsAdaptor()
{
    stopAdaptor();
}

bool SysfsAdaptor::addPath(const QString& path, const int id)
{
    qDebug() << NodeBase::id() << Q_FUNC_INFO << path;

    if (!QFile::exists(path)) {
        return false;
    }

    m_paths.append(path);
    m_pathIds.append(id);

    return true;
}

bool SysfsAdaptor::isRunning() const
{
    return m_running;
}

bool SysfsAdaptor::startAdaptor()
{
    sensordLogD() << "Starting adaptor: " << id();
    return true;
}

void SysfsAdaptor::stopAdaptor()
{
    sensordLogD() << "Stopping adaptor: " << id();
    if ( getAdaptedSensor()->isRunning() )
        stopSensor();
}

bool SysfsAdaptor::startSensor()
{
    AdaptedSensorEntry *entry = getAdaptedSensor();

    if (entry == NULL) {
        sensordLogW() << id() << "Sensor not found: " << name();
        return false;
    }

    // Increase listener count
    entry->addReference();

    /// Check from entry
    if (isRunning()) {
        return false;
    }

    m_shouldBeRunning = true;

    // Do not open if in standby mode.
    if (m_inStandbyMode && !deviceStandbyOverride()) {
        return false;
    }

    /// We are waking up from standby or starting fresh, no matter
    m_inStandbyMode = false;

    if (!startReaderThread()) {
        sensordLogW() << id() << "Failed to start adaptor " << name();
        entry->removeReference();
        entry->setIsRunning(false);
        m_running = false;
        m_shouldBeRunning = false;
        return false;
    }

    entry->setIsRunning(true);
    m_running = true;

    return true;
}

void SysfsAdaptor::stopSensor()
{
    AdaptedSensorEntry *entry = getAdaptedSensor();

    if (entry == NULL) {
        sensordLogW() << id() << "Sensor not found " << name();
        return;
    }

    entry->removeReference();
    if (entry->referenceCount() <= 0) {
        if (!m_inStandbyMode) {
            stopReaderThread();
            closeAllFds();
        }
        entry->setIsRunning(false);
        m_running = false;
    }
}

bool SysfsAdaptor::standby()
{
    sensordLogD() << "Adaptor '" << id() << "' requested to go to standby";
    if (m_inStandbyMode) {
        sensordLogD() << "Adaptor '" << id() << "' not going to standby: already in standby";
        return false;
    }
    if (deviceStandbyOverride()) {
        sensordLogD() << "Adaptor '" << id() << "' not going to standby: overriden";
        return false;
    }
    if (!isRunning()) {
        sensordLogD() << "Adaptor '" << id() << "' not going to standby: not running";
        return false;
    }

    m_inStandbyMode = true;
    m_shouldBeRunning = true;
    sensordLogD() << "Adaptor '" << id() << "' going to standby";
    stopReaderThread();
    closeAllFds();

    m_running = false;
    stopAdaptor();
    return true;
}

bool SysfsAdaptor::resume()
{
    sensordLogD() << "Adaptor '" << id() << "' requested to resume from standby";

    // Don't resume if not in standby
    if (!m_inStandbyMode) {
        sensordLogD() << "Adaptor '" << id() << "' not resuming: not in standby";
        return false;
    }


    if (!m_shouldBeRunning) {
        sensordLogD() << "Adaptor '" << id() << "' not resuming from standby: not running";
        return false;
    }

    sensordLogD() << "Adaptor '" << id() << "' resuming from standby";
    m_inStandbyMode = false;

    if (!startReaderThread()) {
        sensordLogW() << "Adaptor '" << id() << "' failed to resume from standby!";
        return false;
    }

    m_running = true;
    startAdaptor();
    return true;
}

bool SysfsAdaptor::openFds()
{
    QMutexLocker locker(&m_mutex);

    int fd;
    for (int i = 0; i < m_paths.size(); i++) {
        if ((fd = open(m_paths.at(i).toLatin1().constData(), O_RDONLY)) == -1) {
            sensordLogW() << id() << "open(): " << strerror(errno);
            return false;
        }
        m_sysfsDescriptors.append(fd);
    }

    // Set up epoll for select mode
    if (m_mode == SelectMode) {

        if (pipe(m_pipeDescriptors) == -1 ) {
            sensordLogW() << id() << "pipe(): " << strerror(errno);
            return false;
        }

        if (fcntl(m_pipeDescriptors[0], F_SETFD, FD_CLOEXEC) == -1) {
            sensordLogW() << id() << "fcntl(): " << strerror(errno);
            return false;
        }

        // Set up epoll fd
        if ((m_epollDescriptor = epoll_create(m_sysfsDescriptors.size() + 1)) == -1) {
            sensordLogW() << id() << "epoll_create(): " << strerror(errno);
            return false;
        }

        struct epoll_event ev;
        memset(&ev, 0, sizeof(epoll_event));
        ev.events  = EPOLLIN;

        // Set up epolling for the list
        for (int i = 0; i < m_sysfsDescriptors.size(); ++i) {
            ev.data.fd = m_sysfsDescriptors.at(i);
            if (epoll_ctl(m_epollDescriptor, EPOLL_CTL_ADD, m_sysfsDescriptors.at(i), &ev) == -1) {
                sensordLogW() << id() << "epoll_ctl(): " << strerror(errno);
                return false;
            }
        }

        // Add control pipe to poll list
        ev.data.fd = m_pipeDescriptors[0];
        if (epoll_ctl(m_epollDescriptor, EPOLL_CTL_ADD, m_pipeDescriptors[0], &ev) == -1) {
            sensordLogW() << id() << "epoll_ctl(): " << strerror(errno);
            return false;
        }
    }

    return true;
}

void SysfsAdaptor::closeAllFds()
{
    QMutexLocker locker(&m_mutex);

    /* Epoll */
    if (m_epollDescriptor != -1) {
        close(m_epollDescriptor);
        m_epollDescriptor = -1;
    }

    /* Pipe */
    for (int i = 0; i < 2; ++i) {
        if (m_pipeDescriptors[i] != -1) {
            close(m_pipeDescriptors[i]);
            m_pipeDescriptors[i] = -1;
        }
    }

    /* SysFS */
    while (!m_sysfsDescriptors.empty()) {
        if (m_sysfsDescriptors.last() != -1) {
            close(m_sysfsDescriptors.last());
        }
        m_sysfsDescriptors.removeLast();
    }
}

void SysfsAdaptor::stopReaderThread()
{
    if (m_mode == SelectMode) {
        quint64 dummy = 1;
        ssize_t bytesWritten = write(m_pipeDescriptors[1], &dummy, 8);
        if (!bytesWritten)
            qWarning() << id() << "Could not write pipe descriptors";
    }
    else
        m_reader.stopReader();
    m_reader.wait();
}

bool SysfsAdaptor::startReaderThread()
{
    if (!openFds()) {

        closeAllFds();
        return false;
    }

    m_reader.startReader();

    return true;
}

bool SysfsAdaptor::writeToFile(const QByteArray& path, const QByteArray& content)
{
    sensordLogT() << "Writing to '" << path << ": " << content;
    if (!QFile::exists(path))
    {
        sensordLogW() << "Path does not exists: " << path;
        return false;
    }
    int fd = open(path.constData(), O_WRONLY);
    if (fd == -1)
    {
        sensordLogW() << "Failed to open '" << path << "': " << strerror(errno);
        return false;
    }

    if (write(fd, content.constData(), content.size() * sizeof(char)) == -1) {
        close(fd);
        return false;
    }

    close(fd);

    return true;
}

QByteArray SysfsAdaptor::readFromFile(const QByteArray& path)
{
    QFile file(path);
    if (!file.exists(path) || !(file.open(QIODevice::ReadOnly)))
    {
        sensordLogW() << "Path does not exists or open file failed: " << path;
        return QByteArray();
    }

    QByteArray data(file.readAll());
    sensordLogT() << "Read from '" << path << ": " << data;
    return data;
}

bool SysfsAdaptor::checkIntervalUsage() const
{
    if (m_mode == SysfsAdaptor::SelectMode)
    {
        const QList<DataRange>& list = getAvailableIntervals();
        if (list.size() > 1 || (list.size() == 1 && list.first().min != list.first().max))
        {
            sensordLogW() << id() << "Attempting to use IntervalMode interval() function for adaptor in SelectMode. Must reimplement!";
            return false;
        }
    }
    return true;
}

unsigned int SysfsAdaptor::interval() const
{
    if(!checkIntervalUsage())
        return 0;
    return m_interval_us;
}

bool SysfsAdaptor::setInterval(const int sessionId, const unsigned int interval_us)
{
    Q_UNUSED(sessionId);
    if(!checkIntervalUsage())
        return false;
    m_interval_us = interval_us;
    return true;
}

SysfsAdaptor::PollMode SysfsAdaptor::mode() const
{
    return m_mode;
}

SysfsAdaptorReader::SysfsAdaptorReader(SysfsAdaptor *parent) : m_running(false), m_parent(parent)
{
}

void SysfsAdaptorReader::stopReader()
{
    m_running = false;
}

void SysfsAdaptorReader::startReader()
{
    m_running = true;
    start();
}

void SysfsAdaptorReader::run()
{
    while (m_running) {

        if (m_parent->m_mode == SysfsAdaptor::SelectMode) {

            struct epoll_event events[m_parent->m_sysfsDescriptors.size() + 1];
            memset(events, 0x0, sizeof(events));

            int descriptors = epoll_wait(m_parent->m_epollDescriptor, events, m_parent->m_sysfsDescriptors.size() + 1, -1);

            if (descriptors == -1) {
                sensordLogD() << m_parent->id() << "epoll_wait(): " << strerror(errno);
                QThread::msleep(1000);
            } else {
                bool errorInInput = false;
                for (int i = 0; i < descriptors; ++i) {
                    if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                        //Note: we ignore error so the sensordiverter.sh works. This should be handled better when testcases are improved.
                        sensordLogD() << m_parent->id() << "epoll_wait(): error in input fd";
                        errorInInput = true;
                    }
                    int index = m_parent->m_sysfsDescriptors.lastIndexOf(events[i].data.fd);
                    if (index != -1) {
                        m_parent->processSample(m_parent->m_pathIds.at(index), events[i].data.fd);

                        if (m_parent->m_doSeek)
                        {
                            if (lseek(events[i].data.fd, 0, SEEK_SET) == -1)
                            {
                                sensordLogW() << m_parent->id() << "Failed to lseek fd: " << strerror(errno);
                                QThread::msleep(1000);
                            }
                        }
                    } else if (events[i].data.fd == m_parent->m_pipeDescriptors[0]) {
                        m_running = false;
                    }
                }
                if (errorInInput)
                    QThread::msleep(50);
            }
        } else { //IntervalMode

            // Read through all fds.
            for (int i = 0; i < m_parent->m_sysfsDescriptors.size(); ++i) {
                m_parent->processSample(m_parent->m_pathIds.at(i), m_parent->m_sysfsDescriptors.at(i));

                if (m_parent->m_doSeek)
                {
                    if (lseek(m_parent->m_sysfsDescriptors.at(i), 0, SEEK_SET) == -1)
                    {
                        sensordLogW() << m_parent->id() << "Failed to lseek fd: " << strerror(errno);
                        QThread::msleep(1000);
                    }
                }
            }

            // Sleep for interval
            int interval_ms = (m_parent->m_interval_us + 999) / 1000;
            QThread::msleep(interval_ms);
        }
    }
}

void SysfsAdaptor::init()
{
    QString path = SensorFrameworkConfig::configuration()->value(name() + "/path").toString();
    if(!path.isEmpty())
    {
        addPath(path);
    }
    else
    {
        sensordLogW() << id() << "No sysfs path defined for: " << name();
    }
    m_mode = (PollMode)SensorFrameworkConfig::configuration()->value<int>(name() + "/mode", m_mode);
    m_doSeek = SensorFrameworkConfig::configuration()->value<bool>(name() + "/seek", m_doSeek);

    introduceAvailableDataRanges(name());
    introduceAvailableIntervals(name());
    int interval_ms = SensorFrameworkConfig::configuration()->value<int>(name() + "/default_interval", 0);
    if (interval_ms > 0) {
        unsigned int interval_us = (unsigned int)interval_ms * 1000u;
        setDefaultInterval(interval_us);
    }
}
