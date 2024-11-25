/**
   @file sysfsadaptor.h
   @brief Base class for device adaptors reading data from files

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
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

#ifndef SYSFSADAPTOR_H
#define SYSFSADAPTOR_H

#include "deviceadaptor.h"
#include "deviceadaptorringbuffer.h"
#include <QString>
#include <QStringList>
#include <QThread>
#include <QMutex>
#include <QFile>

class SysfsAdaptor;

/**
 * Threading functionality for SysfsAdaptor. Helper class that removes
 * ambiguous inheritance from QObject. Should not be invoked directly
 * by anything except #SysfsAdaptor.
 */
class SysfsAdaptorReader : public QThread
{
    Q_OBJECT
    Q_DISABLE_COPY(SysfsAdaptorReader)

public:
    /**
     * Constructor.
     *
     * @param parent Parent object.
     */
    SysfsAdaptorReader(SysfsAdaptor *parent);

    /**
     * Reader thread entry-function.
     */
    void run();

    /**
     * Initiate reader stopping.
     */
    void stopReader();

    /**
     * Initiate reader starting.
     */
    void startReader();

private:
    bool          m_running; /**< should thread be running or not */
    SysfsAdaptor *m_parent;  /**< parent object. */
};

/**
 * @brief Base class for adaptors accessing device drivers through sysfs.
 *
 * Two different polling modes are supported:
 * <ul>
 *   <li><tt>SysfsAdaptor::IntervalMode</tt> - Read constantly by given frequency (ms delay between reads).</li>
 *   <li><tt>SysfsAdaptor::SelectMode</tt>   - Wait for interrupt from driver before reading.</li>
 * </ul>
 *
 * Simultaneous monitoring of several files is supported by giving unique
 * index for each file.
 */
class SysfsAdaptor : public DeviceAdaptor
{
public:
    enum PollMode {
        SelectMode = 0, /**< Wait for interrupt from driver before reading. */
        IntervalMode    /**< Read constantly with given frequency. */
    };

    /**
     * Constructor.
     *
     * @param id   Identifier string for the adaptor.
     * @param mode Mode to use for monitoring.
     * @param seek Whether lseek() should be called to rewind the monitored fds.
     * @param path Path to the sysfs file device to monitor.
     * @param pathId Identifier for the path (used as parameter to processSample).
     */
    SysfsAdaptor(const QString& id, PollMode mode = SelectMode, bool seek = true,
                 const QString& path = QString(), const int pathId = 0);

    /**
     * Destructor.
     */
    virtual ~SysfsAdaptor();

    virtual void init();

    /**
     * Add a new file device for monitoring. Adaptor must be restarted to
     * get the newly added path into monitoring list.
     * @param path Path to the sysfs file device to monitor.
     * @param id   Identified for the path (used as parameter to processSample).
     * @return     True on success, false otherwise.
     */
    bool addPath(const QString& path, const int id = 0);

    /**
     * Start adaptor and open required resources.
     *
     * @return was adaptor started succesfully.
     */
    virtual bool startAdaptor();

    /**
     * Is adaptor running.
     *
     * @return whether the adaptor is running or not.
     */
    bool isRunning() const;

    /**
     * Stop adaptor and close acquired resources.
     */
    virtual void stopAdaptor();

    virtual bool startSensor();
    virtual void stopSensor();

    virtual bool standby();

    virtual bool resume();

protected:
    /**
     * Called when new data is available on some file descriptor.
     * Must be implemented by the child class.
     *
     * @param pathId Path ID for the file that has received new data.
     *               If path ID was not set when file path was added,
     *               0 will be used.
     * @param fd     Open file descriptor where the new data can be read. This
     *               file descriptor must not be closed!
     */
    virtual void processSample(int pathId, int fd) = 0;

    /**
     * Utility function for writing to files. Can be used to control
     * sensor driver parameters (setting to powersave mode etc.)
     *
     * @param path    Path of the file to write to
     * @param content What to write
     * @return True on success, false on failure.
     */
    static bool writeToFile(const QByteArray& path, const QByteArray& content);

    /**
     * Utility function for reading from sysfs entries.
     *
     * @param path    Path of the file to read from
     * @return Content of the file
     */
    static QByteArray readFromFile(const QByteArray& path);

protected:
    /**
     * Returns the current interval. Valid for PollMode.
     * Reimplementation for adaptors using SelectMode is a must.
     *
     * @return Currently used interval for adaptor.
     */
    virtual unsigned int interval() const;

    /**
     * Sets the interval for the adaptor. This function is valid for
     * adaptors using PollMode. It just sets the number of milliseconds
     * that the adaptor will sleep between reads.
     *
     * For adaptors using SelectMode, reimplementation is a must as this
     * implementatino will have no effect on the behavior.
     *
     * @param value Interval to sleep (ms) between reads
     * @param sessionId Id of the session where the requested originated.
     *        This should be passed with any requests made from this function
     *        to allow for proper state maintenance.
     * @return \c true on successfull set (valid value), \c false otherwise.
     */
    virtual bool setInterval(const int sessionId, const unsigned int interval_us);

    /**
     * Tells which mode the adaptor is using for getting input.
     * Mode is usually set in constructor or via configuration file.
     * @return SysfsAdaptor::PollMode matching the used mode.
     */
    PollMode mode() const;

private:
    /**
     * Opens all file descriptors required by the adaptor.
     *
     * @return were files opened succesfully.
     */
    bool openFds();

    /**
     * Closes all file descriptors.
     *
     * @return were files closed succesfully.
     */
    void closeAllFds();

    /**
     * Stop reader thread.
     */
    void stopReaderThread();

    /**
     * Start reader thread.
     *
     * @return was thread started succesfully.
     */
    bool startReaderThread();

    /**
     * Sanity check for inteval usage.
     */
    bool checkIntervalUsage() const;

    SysfsAdaptorReader  m_reader; /**< reader thread instance */
    PollMode            m_mode;   /**< used poll mode */
    int                 m_epollDescriptor;    /**< open epoll descriptors */
    int                 m_pipeDescriptors[2]; /**< open pipe descriptors */
    QStringList         m_paths;   /**< added paths. */
    QList<int>          m_pathIds; /**< added path IDs. */
    unsigned int m_interval_us; /**< used interval */
    bool m_inStandbyMode;    /**< are we in standby */
    bool m_running;          /**< are we running */
    bool m_shouldBeRunning;  /**< should we be running */
    bool m_doSeek;           /**< should lseek() be performed after reading */
    QList<int> m_sysfsDescriptors; /**< List of open file descriptors. */
    QMutex m_mutex;          /**< mutex protecting starting and stopping. */

    friend class SysfsAdaptorReader;
};

#endif
