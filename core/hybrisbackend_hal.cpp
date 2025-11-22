/****************************************************************************
**
** Copyright (c) 2013 Jolla Ltd.
** Copyright (c) 2025 Jollyboys Ltd.
** Copyright (c) 2026 Jolla Mobile Ltd
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

#include "hybrisbackend_hal.h"

#include "hybrisadaptor.h"
#include "logging.h"

#include <QCoreApplication>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QFile>
#include <QSocketNotifier>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

HybrisBackend *HybrisBackend::getBackend(HybrisManager *manager)
{
    return new HybrisBackendHal(manager);
}

HybrisBackendHal::HybrisBackendHal(HybrisManager *manager)
    : HybrisBackend(manager)
    , m_halModule(NULL)
    , m_halDevice(NULL)
    , m_sensorArray(NULL)
{
}

HybrisBackendHal::~HybrisBackendHal()
{
    if (m_halDevice) {
        qCInfo(lcSensorFw) << "close sensor device";
        int errorCode = sensors_close_1(m_halDevice);
        if (errorCode != 0) {
            qCWarning(lcSensorFw) << "sensors_close() failed:" << strerror(-errorCode);
        }
        m_halDevice = NULL;
    }
}

void HybrisBackendHal::initialize()
{
    int err;

    /* Open android sensor plugin */
    for (int retries = 4; ;) {
        // Try module loading in throwaway child process
        // so that we can retry from clean slate if needed
        fflush(nullptr);
        pid_t child_pid = fork();

        if (child_pid == 0) {
            // Child process
            const hw_module_t *dummyModule = nullptr;
            err = hw_get_module(SENSORS_HARDWARE_MODULE_ID, &dummyModule);
            _exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        if (child_pid == -1) {
            qCWarning(lcSensorFw) << "w_get_module() probe, fork failed:" << strerror(errno);
            QCoreApplication::exit(EXIT_FAILURE);
            return;
        }

        int status = 0;
        if (waitpid(child_pid, &status, 0) == -1) {
            qCWarning(lcSensorFw) << "w_get_module() probe, waitpid failed:" << strerror(errno);
            QCoreApplication::exit(EXIT_FAILURE);
            return;
        }

        // If probe in child process was successful, do it for real
        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
            err = hw_get_module(SENSORS_HARDWARE_MODULE_ID,
                                (hw_module_t const **)&m_halModule);
            if (err == 0)
                break;

            qCWarning(lcSensorFw) << "hw_get_module() failed:" << strerror(-err);
            m_halModule = nullptr;
            QCoreApplication::exit(EXIT_FAILURE);
            return;
        }

        // Bailout or retry after brief delay
        if (--retries < 0) {
            qCWarning(lcSensorFw) << "hw_get_module() probe failed - giving up";
            QCoreApplication::exit(EXIT_FAILURE);
            return;
        }

        qCWarning(lcSensorFw) << "hw_get_module() probe failed";
        QThread::msleep(2000);
    }

    /* Open android sensor device */
    err = sensors_open_1(&m_halModule->common, &m_halDevice);
    if (err != 0) {
        m_halDevice = 0;
        qCWarning(lcSensorFw) << "sensors_open() failed:" << strerror(-err);
        QCoreApplication::exit(EXIT_FAILURE);
        return;
    }

    /* Get static sensor information */
    m_sensorCount = m_halModule->get_sensors_list(m_halModule, &m_sensorArray);
    if (m_sensorCount <= 0) {
        qCWarning(lcSensorFw) << "no sensors found";
        QCoreApplication::exit(EXIT_FAILURE);
        return;
    }

    m_manager->initManager();
}

bool HybrisBackendHal::needsReaderThread()
{
    return true;
}

bool HybrisBackendHal::needsWakeup(const sensors_event_t *eve)
{
    if (m_manager->typeRequiresWakeup(eve->type)) {
        return true;
    }
    return false;
}

bool HybrisBackendHal::isValid(const sensors_event_t *eve)
{
    if (eve->version != sizeof(sensors_event_t)) {
        qCWarning(lcSensorFw) << QString("incorrect event version (version=%1, expected=%2").arg(eve->version).arg(sizeof(sensors_event_t));
        return false;
    }
    return true;
}

int HybrisBackendHal::handle(int index)
{
    return m_sensorArray[index].handle;
}

int HybrisBackendHal::type(int index)
{
    return m_sensorArray[index].type;
}

bool HybrisBackendHal::isWakeupSensor(int index)
{
#if defined(SENSORS_DEVICE_API_VERSION_1_3)
    if (m_halDevice->common.version >= SENSORS_DEVICE_API_VERSION_1_3) {
        if (m_sensorArray[index].flags & SENSOR_FLAG_WAKE_UP)
            return true;
    } else {
        if (strstr(sensorName(index), "(WAKE_UP)"))
            return true;
    }
#else
    if (strstr(sensorName(index), "(WAKE_UP)"))
        return true;
#endif
    return false;
}

void HybrisBackendHal::initFallbackEvent(int index, sensors_event_t *eve)
{
    eve->version = sizeof *eve;
    eve->sensor  = m_sensorArray[index].handle;
    eve->type    = m_sensorArray[index].type;
    switch (m_sensorArray[index].type) {
    case SENSOR_TYPE_LIGHT:
        // Roughly indoor lightning
        eve->light = 400;
        break;
    case SENSOR_TYPE_PROXIMITY:
        // Not-covered
        eve->distance = m_sensorArray[index].maxRange;
        break;
    default:
        eve->sensor  = 0;
        eve->type    = 0;
        break;
    }
}

const char *HybrisBackendHal::sensorName(int index)
{
    return m_sensorArray[index].name ?: "unknown";
}

int HybrisBackendHal::maxDelay(int index)
{
#ifdef SENSORS_DEVICE_API_VERSION_1_3
    if (m_halDevice->common.version >= SENSORS_DEVICE_API_VERSION_1_3)
        return m_sensorArray[index].maxDelay;
#endif
    return -1;
}

int HybrisBackendHal::minDelay(int index)
{
    return m_sensorArray[index].minDelay;
}

float HybrisBackendHal::maxRange(int index)
{
    return m_sensorArray[index].maxRange;
}

float HybrisBackendHal::resolution(int index)
{
    return m_sensorArray[index].resolution;
}

int HybrisBackendHal::setActive(int handle, bool active)
{
    int error = m_halDevice->activate(&m_halDevice->v0, handle, active);

    if (!error) {
        if (active && m_manager->getDelay(handle) != -1) {
            qCInfo(lcSensorFw, "HYBRIS CTL FORCE DELAY UPDATE");
            int delay_us = m_manager->getDelay(handle);
            m_manager->setDelay(handle, delay_us, false);
        }
    }

    return error;
}

int HybrisBackendHal::setDelay(int handle, int64_t delay_ns)
{
    int error = EBADSLT;
    if (m_halDevice->common.version >= SENSORS_DEVICE_API_VERSION_1_0) {
        if (m_halDevice->batch)
            error = m_halDevice->batch(m_halDevice, handle, 0, delay_ns, 0);
        else if (m_halDevice->setDelay)
            error = m_halDevice->setDelay(&m_halDevice->v0, handle, delay_ns);
    } else {
        if (m_halDevice->setDelay)
            error = m_halDevice->setDelay(&m_halDevice->v0, handle, delay_ns);
        else if (m_halDevice->batch) // Here be dragons
            error = m_halDevice->batch(m_halDevice, handle, 0, delay_ns, 0);
    }

    return error;
}

void HybrisBackendHal::readEvents()
{
    sensors_event_t buffer[maxEvents];

    /* Async cancellation point at android hal poll() */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
    int numEvents = m_halDevice->poll(&m_halDevice->v0, buffer, maxEvents);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
    /* Rate limit in poll() error situations */
    if (numEvents < 0) {
        qCWarning(lcSensorFw) << "android device->poll() failed" << strerror(-numEvents);
        struct timespec ts = { 1, 0 }; // 1000 ms
        do { } while (nanosleep(&ts, &ts) == -1 && errno == EINTR);
        return;
    }
    // Queue received events for processing in main thread
    m_manager->queueEvents(buffer, numEvents);
}
