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

#include "hybrisadaptor.h"
#include "hybrisbackend.h"
#include "deviceadaptor.h"
#include "config.h"

#include <QDebug>
#include <QCoreApplication>
#include <QTimer>

#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include <unistd.h>

/* ========================================================================= *
 * UTILITIES
 * ========================================================================= */

char const *
sensorTypeName(int type)
{
    switch (type) {
    case SENSOR_TYPE_META_DATA:
        return "META_DATA";
    case SENSOR_TYPE_ACCELEROMETER:
        return "ACCELEROMETER";
    case SENSOR_TYPE_MAGNETIC_FIELD:
        return "MAGNETIC_FIELD";
    case SENSOR_TYPE_ORIENTATION:
        return "ORIENTATION";
    case SENSOR_TYPE_GYROSCOPE:
        return "GYROSCOPE";
    case SENSOR_TYPE_LIGHT:
        return "LIGHT";
    case SENSOR_TYPE_PRESSURE:
        return "PRESSURE";
    case SENSOR_TYPE_TEMPERATURE:
        return "TEMPERATURE";
    case SENSOR_TYPE_PROXIMITY:
        return "PROXIMITY";
    case SENSOR_TYPE_GRAVITY:
        return "GRAVITY";
    case SENSOR_TYPE_LINEAR_ACCELERATION:
        return "LINEAR_ACCELERATION";
    case SENSOR_TYPE_ROTATION_VECTOR:
        return "ROTATION_VECTOR";
    case SENSOR_TYPE_RELATIVE_HUMIDITY:
        return "RELATIVE_HUMIDITY";
    case SENSOR_TYPE_AMBIENT_TEMPERATURE:
        return "AMBIENT_TEMPERATURE";
    case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
        return "MAGNETIC_FIELD_UNCALIBRATED";
    case SENSOR_TYPE_GAME_ROTATION_VECTOR:
        return "GAME_ROTATION_VECTOR";
    case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
        return "GYROSCOPE_UNCALIBRATED";
    case SENSOR_TYPE_SIGNIFICANT_MOTION:
        return "SIGNIFICANT_MOTION";
    case SENSOR_TYPE_STEP_DETECTOR:
        return "STEP_DETECTOR";
    case SENSOR_TYPE_STEP_COUNTER:
        return "STEP_COUNTER";
    case SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR:
        return "GEOMAGNETIC_ROTATION_VECTOR";
    case SENSOR_TYPE_HEART_RATE:
        return "HEART_RATE";
    case SENSOR_TYPE_TILT_DETECTOR:
        return "TILT_DETECTOR";
    case SENSOR_TYPE_WAKE_GESTURE:
        return "WAKE_GESTURE";
    case SENSOR_TYPE_GLANCE_GESTURE:
        return "GLANCE_GESTURE";
    case SENSOR_TYPE_PICK_UP_GESTURE:
        return "PICK_UP_GESTURE";
    case SENSOR_TYPE_WRIST_TILT_GESTURE:
        return "WRIST_TILT_GESTURE";
    case SENSOR_TYPE_DEVICE_ORIENTATION:
        return "DEVICE_ORIENTATION";
    case SENSOR_TYPE_POSE_6DOF:
        return "POSE_6DOF";
    case SENSOR_TYPE_STATIONARY_DETECT:
        return "STATIONARY_DETECT";
    case SENSOR_TYPE_MOTION_DETECT:
        return "MOTION_DETECT";
    case SENSOR_TYPE_HEART_BEAT:
        return "HEART_BEAT";
    case SENSOR_TYPE_DYNAMIC_SENSOR_META:
        return "DYNAMIC_SENSOR_META";
    case SENSOR_TYPE_ADDITIONAL_INFO:
        return "ADDITIONAL_INFO";
    case SENSOR_TYPE_LOW_LATENCY_OFFBODY_DETECT:
        return "LOW_LATENCY_OFFBODY_DETECT";
    case SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED:
        return "ACCELEROMETER_UNCALIBRATED";
    case SENSOR_TYPE_HINGE_ANGLE:
        return "HINGE_ANGLE";
    case SENSOR_TYPE_HEAD_TRACKER:
        return "HEAD_TRACKER";
    case SENSOR_TYPE_ACCELEROMETER_LIMITED_AXES:
        return "ACCELEROMETER_LIMITED_AXES";
    case SENSOR_TYPE_GYROSCOPE_LIMITED_AXES:
        return "GYROSCOPE_LIMITED_AXES";
    case SENSOR_TYPE_ACCELEROMETER_LIMITED_AXES_UNCALIBRATED:
        return "ACCELEROMETER_LIMITED_AXES_UNCALIBRATED";
    case SENSOR_TYPE_GYROSCOPE_LIMITED_AXES_UNCALIBRATED:
        return "GYROSCOPE_LIMITED_AXES_UNCALIBRATED";
    case SENSOR_TYPE_HEADING:
        return "HEADING";
    }

    static char buf[32];

    if (type >= SENSOR_TYPE_DEVICE_PRIVATE_BASE)
        snprintf(buf, sizeof buf, "SENSOR_TYPE_PRIVATE_%d", type);
    else
        snprintf(buf, sizeof buf, "SENSOR_TYPE_%d", type);

    return buf;
}

static void ObtainTemporaryWakeLock()
{
    static bool triedToOpen = false;
    static int wakeLockFd = -1;

    if (!triedToOpen) {
        triedToOpen = true;
        wakeLockFd = ::open("/sys/power/wake_lock", O_RDWR);
        if (wakeLockFd == -1) {
            qCWarning(lcSensorFw) << "wake locks not available:" << ::strerror(errno);
        }
    }

    if (wakeLockFd != -1) {
        qCInfo(lcSensorFw) << "wake lock to guard sensor data io";
        static const char m[] = "sensorfwd_pass_data 1000000000\n";
        if (::write(wakeLockFd, m, sizeof m - 1) == -1) {
            qCWarning(lcSensorFw) << "wake locking failed:" << ::strerror(errno);
            ::close(wakeLockFd), wakeLockFd = -1;
        }
    }
}

/* ========================================================================= *
 * HybrisSensorState
 * ========================================================================= */

HybrisSensorState::HybrisSensorState()
    : m_minDelay_us(0)
    , m_maxDelay_us(0)
    , m_delay_us(-1)
    , m_active(-1)
{
    memset(&m_fallbackEvent, 0, sizeof m_fallbackEvent);
}

HybrisSensorState::~HybrisSensorState()
{
}

/* ========================================================================= *
 * HybrisManager
 * ========================================================================= */

/* hybrisManager object is created on demand - which ought to happen
 * well after QCoreApplication object has already been instantiated.
 * Cleanup actions are executed on QCoreApplication::aboutToQuit signal.
 * Destructor gets called after exit from main() and should be as
 * close to nop as possible.
 */
Q_GLOBAL_STATIC(HybrisManager, hybrisManager)

HybrisManager::HybrisManager(QObject *parent)
    : QObject(parent)
    , m_registeredAdaptors()
    , m_eventReaderTid(0)
    , m_sensorState(nullptr)
    , m_indexOfType()
    , m_indexOfHandle()
    , m_eventPipeReadFd(-1)
    , m_eventPipeWriteFd(-1)
    , m_eventPipeNotifier(nullptr)
{
    /* Arrange it so that sensors get stopped on exit from mainloop
     */
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, &HybrisManager::cleanup);

    m_backend = HybrisBackend::getBackend(this);
    if (m_backend) {
        // During initialize m_backend is used so can't be done in constructor
        m_backend->initialize();
    } else {
        QCoreApplication::exit(EXIT_FAILURE);
    }
}

bool HybrisManager::typeRequiresWakeup(int type)
{
    // Sensors which are wake-up sensors by default
    switch (type) {
    case SENSOR_TYPE_PROXIMITY:
    case SENSOR_TYPE_SIGNIFICANT_MOTION:
    case SENSOR_TYPE_TILT_DETECTOR:
    case SENSOR_TYPE_WAKE_GESTURE:
    case SENSOR_TYPE_GLANCE_GESTURE:
    case SENSOR_TYPE_PICK_UP_GESTURE:
    case SENSOR_TYPE_WRIST_TILT_GESTURE:
    case SENSOR_TYPE_LOW_LATENCY_OFFBODY_DETECT:
        return true;
    default:
        // Assumption: private types are going to be something that is utilized
        //             as wakeup sensor and for those we want SENSOR_FLAG_WAKE_UP
        return type >= SENSOR_TYPE_DEVICE_PRIVATE_BASE;
    }
}

void HybrisManager::initManager()
{
    QString sensorTypes = SensorFrameworkConfig::configuration()->value("hybrisQuirks/doubleStopReader", QString());
    for (const QString &iter : sensorTypes.split(" ")) {
        int sensorType = iter.toInt();
        if (sensorType > 0) {
            m_doubleStopReaderQuirkSensorTypes.insert(sensorType);
            qCInfo(lcSensorFw) << "doubleStopReaderQuirk selected for" << sensorTypeName(sensorType);
        }
    }

    /* Initialize sensor data forwarding pipe */
    initEventPipe();

    /* Reserve space for sensor state data */
    m_sensorState = new HybrisSensorState[m_backend->sensorCount()];

    /* Selected sensors to use */
    for (int i = 0; i < m_backend->sensorCount(); ++i) {
        /* Always add to handle -> index mapping */
        m_indexOfHandle.insert(m_backend->handle(i), i);

        // some devices have compass and compass raw,
        // ignore compass raw. compass has range 360
        if (m_backend->type(i) == SENSOR_TYPE_ORIENTATION && m_backend->maxRange(i) != 360)
            continue;

        /* Update type -> index mapping if wake flag requirements are met or we have no candidate yet */
        bool wantWakeup = typeRequiresWakeup(m_backend->type(i));
        bool haveWakeup = m_backend->isWakeupSensor(i);
        if (haveWakeup == wantWakeup || !m_indexOfType.contains(m_backend->type(i)))
            m_indexOfType.insert(m_backend->type(i), i);
    }

    /* Initialize selected sensors */
    for (int i = 0; i < m_backend->sensorCount(); ++i) {
        const char *sensorName = m_backend->sensorName(i);
        bool use = m_indexOfType.value(m_backend->type(i)) == i;

        qCInfo(lcSensorFw) << Q_FUNC_INFO
            << (use ? "SELECT" : "IGNORE")
            << "type:" << m_backend->type(i) << sensorTypeName(m_backend->type(i))
            << "name:" << sensorName;

        if (use) {
            // min/max delay in hal is specified in [us]
            int minDelay_us = m_backend->minDelay(i);
            int maxDelay_us = m_backend->maxDelay(i); // Assume: not defined by hal

            /* If HAL does not define maximum delay, we need to invent
             * something that a) allows sensorfwd logic to see a range
             * instead of a point, b) is unlikely to be wrong enough to
             * cause problems...
             *
             * For now use: minDelay * 2, but at least 1000 ms.
             */

            if (maxDelay_us < 0 && minDelay_us > 0) {
                maxDelay_us = (minDelay_us < 500000) ? 1000000 : (minDelay_us * 2);
                qCInfo(lcSensorFw, "hal does not specify maxDelay, fallback: %d us", maxDelay_us);
            }

            // Positive minDelay means delay /can/ be set - but depending
            // on sensor hal implementation it can also mean that some
            // delay /must/ be set or the sensor does not start reporting
            // despite being enabled -> as an protection agains clients
            // failing to explicitly set delays / using delays that would
            // get rejected by upper levels of sensorfwd logic -> setup
            // 200 ms delay (capped to reported min/max range).
            if (minDelay_us >= 0) {
                if (maxDelay_us < minDelay_us)
                    maxDelay_us = minDelay_us;

                int delay_us = minDelay_us ? 200000 : 0;
                if (delay_us < minDelay_us)
                    delay_us = minDelay_us;
                else if (delay_us > maxDelay_us)
                    delay_us = maxDelay_us;

                m_sensorState[i].m_minDelay_us = minDelay_us;
                m_sensorState[i].m_maxDelay_us = maxDelay_us;

                setDelay(m_backend->handle(i), delay_us, true);

                qCInfo(lcSensorFw, "delay = %d [%d, %d]",
                       m_sensorState[i].m_delay_us,
                       m_sensorState[i].m_minDelay_us,
                       m_sensorState[i].m_maxDelay_us);
            }
            m_indexOfType.insert(m_backend->type(i), i);

            /* Set sane fallback values for select sensors in case the
             * hal does not report initial values. */

            sensors_event_t *eve = &m_sensorState[i].m_fallbackEvent;
            m_backend->initFallbackEvent(i, eve);
        }

        /* Make sure all sensors are initially in stopped state */
        setActive(m_backend->handle(i), false);
    }

    if (m_backend->needsReaderThread()) {
        int err;
        /* Start android sensor event reader */
        err = pthread_create(&m_eventReaderTid, 0, eventReaderThread, this);
        if (err) {
            m_eventReaderTid = 0;
            qCCritical(lcSensorFw) << "Failed to start event reader thread";
            return;
        }
        qCInfo(lcSensorFw) << "Event reader thread started";
    }
}

HybrisManager::~HybrisManager()
{
    /* This is exectuted after exiting main() function.
     * No actions that need core application, binder ipc,
     * android hal libraries, etc should be made.
     */
}

void HybrisManager::cleanup()
{
    /* Stop any sensors that are active
     */

    qCInfo(lcSensorFw) << "stop all sensors";
    foreach (HybrisAdaptor *adaptor, m_registeredAdaptors.values()) {
        adaptor->stopSensor();
    }

    if (m_backend) {
        delete m_backend;
        m_backend = nullptr;
    }

    /* Stop reacting to async events
     */

    if (m_eventReaderTid) {
        qCInfo(lcSensorFw) << "Canceling event reader thread";
        int err = pthread_cancel(m_eventReaderTid);
        if (err) {
            qCCritical(lcSensorFw) << "Failed to cancel event reader thread";
        } else {
            qCInfo(lcSensorFw) << "Waiting for event reader thread to exit";
            void *ret = 0;
            struct timespec tmo = { 0, 0};
            clock_gettime(CLOCK_REALTIME, &tmo);
            tmo.tv_sec += 3;
            err = pthread_timedjoin_np(m_eventReaderTid, &ret, &tmo);
            if (err) {
                qCCritical(lcSensorFw) << "Event reader thread did not exit";
            } else {
                qCInfo(lcSensorFw) << "Event reader thread terminated";
                m_eventReaderTid = 0;
            }
        }
        if (m_eventReaderTid) {
            /* The reader thread is stuck.
             * Continuing would be likely to release resourse
             * still in active use and lead to segfaulting.
             * Resort to doing a quick and dirty exit. */
            _exit(EXIT_FAILURE);
        }
    }

    /* Release remaining dynamic resources
     */

    delete[] m_sensorState;
    m_sensorState = nullptr;

    /* Close sensor data transfer pipe */
    cleanupEventPipe();
}

HybrisManager *HybrisManager::instance()
{
    HybrisManager *priv = hybrisManager();
    return priv;
}

int HybrisManager::handleForType(int sensorType) const
{
    int index = indexForType(sensorType);
    return (index < 0) ? -1 : m_backend->handle(index);
}

sensors_event_t *HybrisManager::eventForHandle(int handle) const
{
    sensors_event_t *event = 0;
    int index = indexForHandle(handle);
    if (index != -1) {
        event = &m_sensorState[index].m_fallbackEvent;
    }
    return event;
}

int HybrisManager::indexForHandle(int handle) const
{
    int index = m_indexOfHandle.value(handle, -1);
    if (index == -1)
        qCWarning(lcSensorFw, "HYBRIS CTL invalid sensor handle: %d", handle);
    return index;
}

int HybrisManager::indexForType(int sensorType) const
{
    int index = m_indexOfType.value(sensorType, -1);
    if (index == -1)
        qCWarning(lcSensorFw, "HYBRIS CTL invalid sensor type: %d", sensorType);
    return index;
}

void HybrisManager::startReader(HybrisAdaptor *adaptor)
{
    if (m_registeredAdaptors.values().contains(adaptor)) {
        qCInfo(lcSensorFw) << "activating " << adaptor->name() << adaptor->m_sensorHandle;
        if (!setActive(adaptor->m_sensorHandle, true)) {
            qCWarning(lcSensorFw) << Q_FUNC_INFO << "failed";
            adaptor->setValid(false);
        }
    }
}

void HybrisManager::stopReader(HybrisAdaptor *adaptor)
{
    if (m_registeredAdaptors.values().contains(adaptor)) {
        qCInfo(lcSensorFw) << "deactivating " << adaptor->name();
        if (!setActive(adaptor->m_sensorHandle, false)) {
            qCWarning(lcSensorFw) << Q_FUNC_INFO << "failed";
        } else if (m_doubleStopReaderQuirkSensorTypes.contains(adaptor->m_sensorType)) {
            /* For example: in C2 stopping gyroscope can cause accelerometer
             * reporting to freeze. Situation can be remedied by doing an
             * extra gyroscope enable and disable with forced datarate change.
             *
             * This behavior needs to be enabled in sensorfwd configuration.
             */
            qCInfo(lcSensorFw) << "doubleStopReaderQuirk executed for" << sensorTypeName(adaptor->m_sensorType);
            HybrisSensorState *state = &m_sensorState[indexForHandle(adaptor->m_sensorHandle)];
            int curr_delay_us = state->m_delay_us;
            int temp_delay_us = 100000;
            if (temp_delay_us == curr_delay_us)
                temp_delay_us *= 2;
            setDelay(adaptor->m_sensorHandle, temp_delay_us, false);
            setActive(adaptor->m_sensorHandle, true);
            setActive(adaptor->m_sensorHandle, false);
            setDelay(adaptor->m_sensorHandle, curr_delay_us, false);
        }
    }
}

void HybrisManager::processSample(const sensors_event_t& data)
{
    foreach (HybrisAdaptor *adaptor, m_registeredAdaptors.values(data.type)) {
        if (adaptor->isRunning()) {
            adaptor->processSample(data);
        }
    }
}

void HybrisManager::registerAdaptor(HybrisAdaptor *adaptor)
{
    if (!m_registeredAdaptors.values().contains(adaptor) && adaptor->isValid()) {
        m_registeredAdaptors.insert(adaptor->m_sensorType, adaptor);
    }
}

float HybrisManager::scaleSensorValue(const float value, const int type) const
{
    float outValue;
    switch (type) {
    case SENSOR_TYPE_ACCELEROMETER:
    case SENSOR_TYPE_GRAVITY:
    case SENSOR_TYPE_LINEAR_ACCELERATION:
        //sensorfw wants milli-G'
        outValue = value * GRAVITY_RECIPROCAL_THOUSANDS;
        break;
    case SENSOR_TYPE_MAGNETIC_FIELD:
    case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
        // uT to nT
        outValue = value * 1000;
        break;
    case SENSOR_TYPE_GYROSCOPE:
    case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
        // From rad/s to mdeg/s
        outValue = value * RADIANS_TO_DEGREES * 1000;
        break;
    case SENSOR_TYPE_PRESSURE:
        // From hPa to Pa
        outValue = value * 100;
        break;
    default:
        outValue = value;
        break;
    }
    return outValue;
}

float HybrisManager::getMaxRange(int handle) const
{
    float range = 0;
    int index = indexForHandle(handle);

    if (index != -1) {
        range = scaleSensorValue(m_backend->maxRange(index), m_backend->type(index));
        qCDebug(lcSensorFw, "HYBRIS CTL getMaxRange(%d=%s) -> %g",
                m_backend->handle(index), sensorTypeName(m_backend->type(index)), range);
    }

    return range;
}

float HybrisManager::getResolution(int handle) const
{
    float resolution = 0;
    int index = indexForHandle(handle);

    if (index != -1) {
        resolution = scaleSensorValue(m_backend->resolution(index), m_backend->type(index));
        qCDebug(lcSensorFw, "HYBRIS CTL getResolution(%d=%s) -> %g",
                m_backend->handle(index), sensorTypeName(m_backend->type(index)), resolution);
    }

    return resolution;
}

int HybrisManager::getMinDelay(int handle) const
{
    int delay_us = 0;
    int index = indexForHandle(handle);

    if (index != -1) {
        HybrisSensorState     *state  = &m_sensorState[index];

        delay_us = state->m_minDelay_us;
        qCDebug(lcSensorFw, "HYBRIS CTL getMinDelay(%d=%s) -> %d",
                m_backend->handle(index), sensorTypeName(m_backend->type(index)), delay_us);
    }

    return delay_us;
}

int HybrisManager::getMaxDelay(int handle) const
{
    int delay_us = 0;
    int index = indexForHandle(handle);

    if (index != -1) {
        HybrisSensorState     *state  = &m_sensorState[index];

        delay_us = state->m_maxDelay_us;
        qCDebug(lcSensorFw, "HYBRIS CTL getMaxDelay(%d=%s) -> %d",
                m_backend->handle(index), sensorTypeName(m_backend->type(index)), delay_us);
    }

    return delay_us;
}

int HybrisManager::getDelay(int handle) const
{
    int delay_us = 0;
    int index = indexForHandle(handle);

    if (index != -1) {
        HybrisSensorState     *state  = &m_sensorState[index];

        delay_us = state->m_delay_us;
        qCDebug(lcSensorFw, "HYBRIS CTL getDelay(%d=%s) -> %d",
                m_backend->handle(index), sensorTypeName(m_backend->type(index)), delay_us);
    }

    return delay_us;
}

bool HybrisManager::setDelay(int handle, int delay_us, bool force)
{
    bool success = false;
    int index = indexForHandle(handle);

    if (index != -1) {
        HybrisSensorState     *state  = &m_sensorState[index];
        int handle = m_backend->handle(index);
        int type = m_backend->type(index);

        if (!force && state->m_delay_us == delay_us) {
            qCDebug(lcSensorFw, "HYBRIS CTL setDelay(%d=%s, %d) -> no-change",
                    handle, sensorTypeName(type), delay_us);
            success = true;
        } else {
            int64_t delay_ns = delay_us * 1000LL;
            int error = m_backend->setDelay(handle, delay_ns);
            if (error) {
                qCWarning(lcSensorFw, "HYBRIS CTL setDelay(%d=%s, %d) -> %d=%s",
                          handle, sensorTypeName(type), delay_us,
                          error, strerror(error));
            } else {
                qCInfo(lcSensorFw, "HYBRIS CTL setDelay(%d=%s, %d) -> success",
                       handle, sensorTypeName(type), delay_us);
                state->m_delay_us = delay_us;
                success = true;
            }
        }
    }

    return success;
}

bool HybrisManager::getActive(int handle) const
{
    bool active = false;
    int index = indexForHandle(handle);

    if (index != -1) {
        HybrisSensorState     *state  = &m_sensorState[index];

        active = (state->m_active > 0);
        qCDebug(lcSensorFw, "HYBRIS CTL getActive(%d=%s) -> %s",
                m_backend->handle(index), sensorTypeName(m_backend->type(index)),
                active ? "true" : "false");
    }
    return active;
}

bool HybrisManager::setActive(int handle, bool active)
{
    bool success = false;
    int index = indexForHandle(handle);

    if (index != -1) {
        HybrisSensorState     *state  = &m_sensorState[index];
        int handle = m_backend->handle(index);
        int type = m_backend->type(index);

        if (state->m_active == active) {
            qCDebug(lcSensorFw, "HYBRIS CTL setActive(%d=%s, %s) -> no-change",
                    handle, sensorTypeName(type), active ? "true" : "false");
            success = true;
        } else {
            int error = m_backend->setActive(handle, active);
            if (error) {
                qCWarning(lcSensorFw, "HYBRIS CTL setActive(%d=%s, %s) -> %d=%s", handle,
                          sensorTypeName(type), active ? "true" : "false", error, strerror(error));
            } else {
                qCInfo(lcSensorFw, "HYBRIS CTL setActive(%d=%s, %s) -> success", handle,
                       sensorTypeName(type), active ? "true" : "false");
                state->m_active = active;
                success = true;
            }
        }
    }
    return success;
}

void *HybrisManager::eventReaderThread(void *aptr)
{
    HybrisManager *manager = static_cast<HybrisManager *>(aptr);

    /* Async cancellation, but disabled */
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
    /* Leave INT/TERM signal processing up to the main thread */
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGINT);
    sigaddset(&ss, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &ss, 0);
    /* Loop until explicitly canceled */
    for (;;) {
        manager->m_backend->readEvents();
    }
    return 0;
}

void HybrisManager::initEventPipe()
{
    qCInfo(lcSensorFw, "initialize event pipe");
    int pfd[2] = {-1, -1};
    if (::pipe2(pfd, O_CLOEXEC) == -1) {
        qCWarning(lcSensorFw, "failed to create event pipe: %s", strerror(errno));
    } else {
        m_eventPipeReadFd = pfd[0];
        m_eventPipeWriteFd = pfd[1];
        m_eventPipeNotifier = new QSocketNotifier(m_eventPipeReadFd, QSocketNotifier::Read);
        QObject::connect(m_eventPipeNotifier, &QSocketNotifier::activated, this, &HybrisManager::eventPipeWakeup);
        m_eventPipeNotifier->setEnabled(true);
    }
}

void HybrisManager::cleanupEventPipe()
{
    qCInfo(lcSensorFw, "cleanup event pipe");
    if (m_eventPipeNotifier) {
        delete m_eventPipeNotifier;
        m_eventPipeNotifier = nullptr;
    }
    if (m_eventPipeWriteFd != -1) {
        ::close(m_eventPipeWriteFd);
        m_eventPipeWriteFd = -1;
    }
    if (m_eventPipeReadFd != -1) {
        ::close(m_eventPipeReadFd);
        m_eventPipeReadFd = -1;
    }
}

void HybrisManager::eventPipeWakeup(int fd)
{
    if (m_eventPipeReadFd != fd) {
        m_eventPipeNotifier->setEnabled(false);
        qCWarning(lcSensorFw, "fd mismatch, event pipe notifier disabled");
    } else {
        sensors_event_t events[maxEvents];
        ssize_t rc = ::read(m_eventPipeReadFd, events, sizeof events);
        if (rc == 0) {
            qCWarning(lcSensorFw, "event pipe eof, notifier disabled");
            m_eventPipeNotifier->setEnabled(false);
        } else if (rc == -1) {
            if (errno != EAGAIN && errno != EINTR) {
                qCWarning(lcSensorFw, "event pipe %s, notifier disabled", strerror(errno));
                m_eventPipeNotifier->setEnabled(false);
            }
        } else {
            const int numEvents = static_cast<int>(rc / sizeof *events);
            processEvents(events, numEvents);
        }
    }
}

int HybrisManager::queueEvents(const sensors_event_t *buffer, int numEvents)
{
    /* Pre-checks */
    int wakeupEventCount = 0;
    bool errorInInput = false;
    for (int i = 0; i < numEvents; i++) {
        const sensors_event_t &data = buffer[i];
        qCDebug(lcSensorFw, "QUEUE HYBRIS EVE %s", sensorTypeName(data.type));

        if (!m_backend->isValid(&data)) {
            errorInInput = true;
        }

        if (m_backend->needsWakeup(&data))
            ++wakeupEventCount;
    }

    /* Suspend proof sensor data forwarding */
    if (wakeupEventCount)
        ObtainTemporaryWakeLock();

    /* Forward via pipe for processing in main threah */
    if (!errorInInput && numEvents > 0 && m_eventPipeWriteFd != -1) {
        if (::write(m_eventPipeWriteFd, buffer, numEvents * sizeof *buffer) == -1) {
            qCWarning(lcSensorFw, "event pipe write failure: %s", strerror(errno));
            errorInInput = true;
        }
    }

    /* Rate limit after receiving erraneous events / write failures */
    if (errorInInput) {
        struct timespec ts = { 0, 50 * 1000 * 1000 }; // 50 ms
        do { } while (nanosleep(&ts, &ts) == -1 && errno == EINTR);
    }
    return wakeupEventCount;
}

int HybrisManager::processEvents(const sensors_event_t *buffer, int numEvents)
{
    /* Pre-checks */
    int wakeupEventCount = 0;
    for (int i = 0; i < numEvents; i++) {
        const sensors_event_t& data = buffer[i];
        if (m_backend->needsWakeup(&data))
            ++wakeupEventCount;
    }

    /* Suspend proof sensor data processing */
    if (wakeupEventCount)
        ObtainTemporaryWakeLock();

    /* Push the sensor data down chains */
    for (int i = 0; i < numEvents; i++) {
        const sensors_event_t& data = buffer[i];
        qCDebug(lcSensorFw, "HYBRIS EVE %s", sensorTypeName(data.type));

        /* Got data -> Clear the no longer needed fallback event */
        sensors_event_t *fallback = eventForHandle(data.sensor);
        if (fallback && fallback->type == data.type && fallback->sensor == data.sensor) {
            fallback->type = fallback->sensor = 0;
        }

        processSample(data);
    }
    return wakeupEventCount;
}

/* ========================================================================= *
 * HybrisAdaptor
 * ========================================================================= */

HybrisAdaptor::HybrisAdaptor(const QString& id, int type)
    : DeviceAdaptor(id)
    , m_inStandbyMode(false)
    , m_isRunning(false)
    , m_shouldBeRunning(false)
    , m_sensorHandle(-1)
    , m_sensorType(type)
{
    m_sensorHandle = hybrisManager()->handleForType(m_sensorType);
    if (m_sensorHandle == -1) {
        qCWarning(lcSensorFw) << Q_FUNC_INFO <<"no such sensor" << id;
        setValid(false);
        return;
    }

    introduceAvailableDataRange(DataRange(minRange(), maxRange(), resolution()));
    introduceAvailableInterval(DataRange(minInterval(), maxInterval(), 0));

    hybrisManager()->registerAdaptor(this);
}

HybrisAdaptor::~HybrisAdaptor()
{
}

void HybrisAdaptor::init()
{
}

void HybrisAdaptor::sendInitialData()
{
    // virtual dummy
    // used for ps/als initial value hacks
}

bool HybrisAdaptor::writeToFile(const QByteArray& path, const QByteArray& content)
{
    qCDebug(lcSensorFw) << "Writing to '" << path << ": " << content;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcSensorFw) << "Failed to open '" << path << "': " << file.errorString();
        return false;
    }
    if (file.write(content.constData(), content.size()) == -1) {
        qCWarning(lcSensorFw) << "Failed to write to '" << path << "': " << file.errorString();
        file.close();
        return false;
    }

    file.close();
    return true;
}

/* ------------------------------------------------------------------------- *
 * range
 * ------------------------------------------------------------------------- */

qreal HybrisAdaptor::minRange() const
{
    return 0;
}

qreal HybrisAdaptor::maxRange() const
{
    return hybrisManager()->getMaxRange(m_sensorHandle);
}

qreal HybrisAdaptor::resolution() const
{
    return hybrisManager()->getResolution(m_sensorHandle);
}

/* ------------------------------------------------------------------------- *
 * interval
 * ------------------------------------------------------------------------- */

unsigned int HybrisAdaptor::minInterval() const
{
    return hybrisManager()->getMinDelay(m_sensorHandle);
}

unsigned int HybrisAdaptor::maxInterval() const
{
    return hybrisManager()->getMaxDelay(m_sensorHandle);
}

unsigned int HybrisAdaptor::interval() const
{
    return hybrisManager()->getDelay(m_sensorHandle);
}

bool HybrisAdaptor::setInterval(const int sessionId, const unsigned int interval_us)
{
    Q_UNUSED(sessionId);

    bool ok = hybrisManager()->setDelay(m_sensorHandle, interval_us, false);

    if (!ok) {
        qCWarning(lcSensorFw) << id() << Q_FUNC_INFO << "setInterval not ok";
    } else {
        /* If we have not yet received sensor data, apply fallback value */
        sensors_event_t *fallback = hybrisManager()->eventForHandle(m_sensorHandle);
        if (fallback && fallback->sensor == m_sensorHandle && fallback->type == m_sensorType) {
            qCDebug(lcSensorFw, "HYBRIS FALLBACK type:%s sensor:%d",
                    sensorTypeName(fallback->type),
                    fallback->sensor);
            processSample(*fallback);
            fallback->sensor = fallback->type = 0;
        }

        sendInitialData();
    }

    return ok;
}

/* ------------------------------------------------------------------------- *
 * start/stop adaptor
 * ------------------------------------------------------------------------- */

bool HybrisAdaptor::startAdaptor()
{
    return isValid();
}

void HybrisAdaptor::stopAdaptor()
{
    if (getAdaptedSensor()->isRunning())
        stopSensor();
}

/* ------------------------------------------------------------------------- *
 * start/stop sensor
 * ------------------------------------------------------------------------- */

bool HybrisAdaptor::isRunning() const
{
    return m_isRunning;
}

void HybrisAdaptor::evaluateSensor()
{
    // Get listener object
    AdaptedSensorEntry *entry = getAdaptedSensor();
    if (entry == nullptr) {
        qCWarning(lcSensorFw) << id() << Q_FUNC_INFO << "Sensor not found: " << name();
        return;
    }

    // Check policy
    bool runningAllowed = (deviceStandbyOverride() || !m_inStandbyMode);

    // Target state
    bool startRunning = m_shouldBeRunning && runningAllowed;

    if (m_isRunning != startRunning) {
        if ((m_isRunning = startRunning)) {
            hybrisManager()->startReader(this);
            if (entry->addReference() == 1) {
                entry->setIsRunning(true);
            }

            /* If we have not yet received sensor data, apply fallback value */
            sensors_event_t *fallback = hybrisManager()->eventForHandle(m_sensorHandle);
            if (fallback && fallback->sensor == m_sensorHandle && fallback->type == m_sensorType) {
                qCDebug(lcSensorFw, "HYBRIS FALLBACK type:%s sensor:%d",
                        sensorTypeName(fallback->type),
                        fallback->sensor);
                processSample(*fallback);
                fallback->sensor = fallback->type = 0;
            }
        } else {
            if (entry->removeReference() == 0) {
                entry->setIsRunning(false);
            }
            hybrisManager()->stopReader(this);
        }
        qCDebug(lcSensorFw) << id() << Q_FUNC_INFO << "entry" << entry->name()
                            << "refs:" << entry->referenceCount() << "running:" << entry->isRunning();
    }
}

bool HybrisAdaptor::startSensor()
{
    // Note: This is overloaded and called by each HybrisXxxAdaptor::startSensor()
    if (!m_shouldBeRunning) {
        m_shouldBeRunning = true;
        qCDebug(lcSensorFw, "%s m_shouldBeRunning = %d", sensorTypeName(m_sensorType), m_shouldBeRunning);
        evaluateSensor();
    }
    return true;
}

void HybrisAdaptor::stopSensor()
{
    // Note: This is overloaded and called by each HybrisXxxAdaptor::stopSensor()
    if (m_shouldBeRunning) {
        m_shouldBeRunning = false;
        qCDebug(lcSensorFw, "%s m_shouldBeRunning = %d", sensorTypeName(m_sensorType), m_shouldBeRunning);
        evaluateSensor();
    }
}

bool HybrisAdaptor::standby()
{
    if (!m_inStandbyMode) {
        m_inStandbyMode = true;
        qCDebug(lcSensorFw, "%s m_inStandbyMode = %d", sensorTypeName(m_sensorType), m_inStandbyMode);
        evaluateSensor();
    }
    return true;
}

bool HybrisAdaptor::resume()
{
    if (m_inStandbyMode) {
        m_inStandbyMode = false;
        qCDebug(lcSensorFw, "%s m_inStandbyMode = %d", sensorTypeName(m_sensorType), m_inStandbyMode);
        evaluateSensor();
    }
    return true;
}
