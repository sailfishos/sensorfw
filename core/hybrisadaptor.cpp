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

#include "hybrisadaptor.h"
#include "deviceadaptor.h"

#include <QDebug>
#include <QCoreApplication>
#include <QTimer>

#ifndef USE_BINDER
#include <hardware/hardware.h>
#endif

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#ifdef USE_BINDER
#define SENSOR_BINDER_SERVICE_DEVICE "/dev/hwbinder"
#define SENSOR_BINDER_SERVICE_IFACE_1_0 "android.hardware.sensors@1.0::ISensors"
#define SENSOR_BINDER_SERVICE_NAME_1_0  SENSOR_BINDER_SERVICE_IFACE_1_0 "/default"
#define SENSOR_BINDER_SERVICE_IFACE_2_0 "android.hardware.sensors@2.0::ISensors"
#define SENSOR_BINDER_SERVICE_NAME_2_0  SENSOR_BINDER_SERVICE_IFACE_2_0 "/default"
#define SENSOR_BINDER_SERVICE_CALLBACK_IFACE_2_0 "android.hardware.sensors@2.0::ISensorsCallback"
#define SENSOR_BINDER_SERVICE_IFACE_2_1 "android.hardware.sensors@2.1::ISensors"
#define SENSOR_BINDER_SERVICE_NAME_2_1  SENSOR_BINDER_SERVICE_IFACE_2_1 "/default"
#define SENSOR_BINDER_SERVICE_CALLBACK_IFACE_2_1 "android.hardware.sensors@2.1::ISensorsCallback"
#define MAX_RECEIVE_BUFFER_EVENT_COUNT 128

static const GBinderClientIfaceInfo sensors_2_client_ifaces[] = {
    {SENSOR_BINDER_SERVICE_IFACE_2_1, INJECT_SENSOR_DATA_2_1 },
    {SENSOR_BINDER_SERVICE_IFACE_2_0, CONFIG_DIRECT_REPORT },
};

G_STATIC_ASSERT
    (G_N_ELEMENTS(sensors_2_client_ifaces) == SENSOR_INTERFACE_2_1);

const char* const sensors_2_callback_ifaces[] = {
    SENSOR_BINDER_SERVICE_CALLBACK_IFACE_2_1,
    SENSOR_BINDER_SERVICE_CALLBACK_IFACE_2_0,
    NULL
};

#endif

namespace {
/* Maximum number of events to transmit over event pipe in one go.
 * Also defines maximum number of events to ask from android hal/service.
 */
const size_t maxEvents = 64;
}

/* ========================================================================= *
 * UTILITIES
 * ========================================================================= */

static char const *
sensorTypeName(int type)
{
    switch (type) {
    case SENSOR_TYPE_META_DATA:                   return "META_DATA";
    case SENSOR_TYPE_ACCELEROMETER:               return "ACCELEROMETER";
    case SENSOR_TYPE_GEOMAGNETIC_FIELD:           return "GEOMAGNETIC_FIELD";
    case SENSOR_TYPE_ORIENTATION:                 return "ORIENTATION";
    case SENSOR_TYPE_GYROSCOPE:                   return "GYROSCOPE";
    case SENSOR_TYPE_LIGHT:                       return "LIGHT";
    case SENSOR_TYPE_PRESSURE:                    return "PRESSURE";
    case SENSOR_TYPE_TEMPERATURE:                 return "TEMPERATURE";
    case SENSOR_TYPE_PROXIMITY:                   return "PROXIMITY";
    case SENSOR_TYPE_GRAVITY:                     return "GRAVITY";
    case SENSOR_TYPE_LINEAR_ACCELERATION:         return "LINEAR_ACCELERATION";
    case SENSOR_TYPE_ROTATION_VECTOR:             return "ROTATION_VECTOR";
    case SENSOR_TYPE_RELATIVE_HUMIDITY:           return "RELATIVE_HUMIDITY";
    case SENSOR_TYPE_AMBIENT_TEMPERATURE:         return "AMBIENT_TEMPERATURE";
    case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED: return "MAGNETIC_FIELD_UNCALIBRATED";
    case SENSOR_TYPE_GAME_ROTATION_VECTOR:        return "GAME_ROTATION_VECTOR";
    case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:      return "GYROSCOPE_UNCALIBRATED";
    case SENSOR_TYPE_SIGNIFICANT_MOTION:          return "SIGNIFICANT_MOTION";
    case SENSOR_TYPE_STEP_DETECTOR:               return "STEP_DETECTOR";
    case SENSOR_TYPE_STEP_COUNTER:                return "STEP_COUNTER";
    case SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR: return "GEOMAGNETIC_ROTATION_VECTOR";
    case SENSOR_TYPE_HEART_RATE:                  return "HEART_RATE";
    case SENSOR_TYPE_TILT_DETECTOR:               return "TILT_DETECTOR";
    case SENSOR_TYPE_WAKE_GESTURE:                return "WAKE_GESTURE";
    case SENSOR_TYPE_GLANCE_GESTURE:              return "GLANCE_GESTURE";
    case SENSOR_TYPE_PICK_UP_GESTURE:             return "PICK_UP_GESTURE";
    case SENSOR_TYPE_WRIST_TILT_GESTURE:          return "WRIST_TILT_GESTURE";
    }

    static char buf[32];
    snprintf(buf, sizeof buf, "type%d", type);
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
            sensordLogW() << "wake locks not available:" << ::strerror(errno);
        }
    }

    if (wakeLockFd != -1) {
        sensordLogD() << "wake lock to guard sensor data io";
        static const char m[] = "sensorfwd_pass_data 1000000000\n";
        if (::write(wakeLockFd, m, sizeof m - 1) == -1) {
            sensordLogW() << "wake locking failed:" << ::strerror(errno);
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
    , m_initialized(false)
    , m_registeredAdaptors()
#ifdef USE_BINDER
    , m_client(NULL)
    , m_deathId(0)
    , m_pollTransactId(0)
    , m_remote(NULL)
    , m_serviceManager(NULL)
    , m_sensorInterfaceEnum(SENSOR_INTERFACE_COUNT)
    , m_sensorCallback(NULL)
#else
    , m_halModule(NULL)
    , m_halDevice(NULL)
#endif
    , m_sensorArray(NULL)
    , m_eventReaderTid(0)
    , m_sensorCount(0)
    , m_sensorState(NULL)
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

#ifdef USE_BINDER
    startConnect();
#else
    int err;

    /* Open android sensor plugin */
    err = hw_get_module(SENSORS_HARDWARE_MODULE_ID,
                        (hw_module_t const**)&m_halModule);
    if (err != 0) {
        m_halModule = 0;
        sensordLogW() << "hw_get_module() failed" <<  strerror(-err);
        return ;
    }

    /* Open android sensor device */
    err = sensors_open_1(&m_halModule->common, &m_halDevice);
    if (err != 0) {
        m_halDevice = 0;
        sensordLogW() << "sensors_open() failed:" << strerror(-err);
        return;
    }

    /* Get static sensor information */
    m_sensorCount = m_halModule->get_sensors_list(m_halModule, &m_sensorArray);

    initManager();
#endif
}

#ifdef USE_BINDER
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
        return false;
    }
}
#endif

void HybrisManager::initManager()
{
    /* Initialize sensor data forwarding pipe */
    initEventPipe();

    /* Reserve space for sensor state data */
    m_sensorState = new HybrisSensorState[m_sensorCount];

    /* Select and initialize sensors to be used */
    for (int i = 0 ; i < m_sensorCount ; i++) {
        /* Always do handle -> index mapping */
        m_indexOfHandle.insert(m_sensorArray[i].handle, i);

        bool use = true;
        // Assumption: The primary sensor variants that we want to
        // use are listed before the secondary ones that we want
        // to ignore -> Use the 1st entry found for each sensor type.
        if (m_indexOfType.contains(m_sensorArray[i].type)) {
            use = false;
        }

        // some devices have compass and compass raw,
        // ignore compass raw. compass has range 360
        if (m_sensorArray[i].type == SENSOR_TYPE_ORIENTATION &&
            m_sensorArray[i].maxRange != 360) {
            use = false;
        }

#ifdef USE_BINDER
        // Pick wake-up variant for the types which are wake-up sensors by default
        if (typeRequiresWakeup(m_sensorArray[i].type)) {
            if ((m_sensorArray[i].flags & SENSOR_FLAG_WAKE_UP) == 0) {
                sensordLogD() << "Ignoring non-wake-up sensor of type " << m_sensorArray[i].type << sensorTypeName(m_sensorArray[i].type);
                use = false;
            }
        } else {
            // All other sensors shall use non-wake-up sensor variant
            if ((m_sensorArray[i].flags & SENSOR_FLAG_WAKE_UP) != 0) {
                sensordLogD() << "Ignoring wake-up sensor of type " << m_sensorArray[i].type << sensorTypeName(m_sensorArray[i].type);
                use = false;
            }
        }
#endif

        sensordLogD() << Q_FUNC_INFO
            << (use ? "SELECT" : "IGNORE")
            << "type:" << m_sensorArray[i].type << sensorTypeName(m_sensorArray[i].type)
#ifdef USE_BINDER
            << "name:" << (m_sensorArray[i].name.data.str ?: "n/a");
#else
            << "name:" << (m_sensorArray[i].name ?: "n/a");
#endif

        if (use) {
            // min/max delay in hal is specified in [us]
            int minDelay_us = m_sensorArray[i].minDelay;
            int maxDelay_us = -1; // Assume: not defined by hal

#ifdef USE_BINDER
            maxDelay_us = m_sensorArray[i].maxDelay;
#else
#ifdef SENSORS_DEVICE_API_VERSION_1_3
            if (m_halDevice->common.version >= SENSORS_DEVICE_API_VERSION_1_3)
                maxDelay_us = m_sensorArray[i].maxDelay;
#endif
#endif

            /* If HAL does not define maximum delay, we need to invent
             * something that a) allows sensorfwd logic to see a range
             * instead of a point, b) is unlikely to be wrong enough to
             * cause problems...
             *
             * For now use: minDelay * 2, but at least 1000 ms.
             */

            if (maxDelay_us < 0 && minDelay_us > 0) {
                maxDelay_us = (minDelay_us < 500000) ? 1000000 : (minDelay_us * 2);
                sensordLogD("hal does not specify maxDelay, fallback: %d us",
                            maxDelay_us);
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

                setDelay(m_sensorArray[i].handle, delay_us, true);

                sensordLogD("delay = %d [%d, %d]",
                            m_sensorState[i].m_delay_us,
                            m_sensorState[i].m_minDelay_us,
                            m_sensorState[i].m_maxDelay_us);
            }
            m_indexOfType.insert(m_sensorArray[i].type, i);

            /* Set sane fallback values for select sensors in case the
             * hal does not report initial values. */

            sensors_event_t *eve = &m_sensorState[i].m_fallbackEvent;
#ifndef USE_BINDER
            eve->version = sizeof *eve;
#endif
            eve->sensor  = m_sensorArray[i].handle;
            eve->type    = m_sensorArray[i].type;
            switch (m_sensorArray[i].type) {
            case SENSOR_TYPE_LIGHT:
                // Roughly indoor lightning
#ifdef USE_BINDER
                eve->u.scalar = 400;
#else
                eve->light = 400;
#endif
                break;

            case SENSOR_TYPE_PROXIMITY:
                // Not-covered
#ifdef USE_BINDER
                eve->u.scalar = m_sensorArray[i].maxRange;
#else
                eve->distance = m_sensorArray[i].maxRange;
#endif
                break;
            default:
                eve->sensor  = 0;
                eve->type    = 0;
                break;
            }
        }

        /* Make sure all sensors are initially in stopped state */
        setActive(m_sensorArray[i].handle, false);
    }

#ifdef USE_BINDER
    if (m_sensorInterfaceEnum == SENSOR_INTERFACE_1_0) {
        pollEvents();
    } else {
#endif
    int err;
    /* Start android sensor event reader */
    err = pthread_create(&m_eventReaderTid, 0, eventReaderThread, this);
    if (err) {
        m_eventReaderTid = 0;
        sensordLogC() << "Failed to start event reader thread";
        return;
    }
    sensordLogD() << "Event reader thread started";

#ifdef USE_BINDER
    }
#else
    m_initialized = true;
#endif
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

    sensordLogD() << "stop all sensors";
    foreach (HybrisAdaptor *adaptor, m_registeredAdaptors.values()) {
        adaptor->stopSensor();
    }

    /* Stop reacting to async events
     */

#ifdef USE_BINDER
    gbinder_remote_object_remove_handler(m_remote, m_deathId);
    m_deathId = 0;

    if (m_pollTransactId) {
        gbinder_client_cancel(m_client, m_pollTransactId);
        m_pollTransactId = 0;

        // The above code just marks down pending POLL transaction as
        // to be cancelled later on when handler thread gets woken up.
        //
        // If we are exiting right after cleanup(), that is never going
        // to happen and gbinder_ipc_exit() cleanup code blocks sensorfwd
        // exit indefinitely.
        //
        // As a workaround: make a dummy POLL transaction, for which a
        // reply is sent immediately, which then wakes up the handler
        // thread, the cancellation gets processed and exit is unblocked.

        GBinderLocalRequest *req = gbinder_client_new_request2(m_client, POLL);
        int32_t status = 0;
        gbinder_local_request_append_int32(req, 0);
        GBinderRemoteReply *reply = gbinder_client_transact_sync_reply(m_client, POLL, req, &status);
        gbinder_remote_reply_unref(reply);
        gbinder_local_request_unref(req);
    }

    gbinder_local_object_unref(m_sensorCallback);
    m_sensorCallback = NULL;
#endif

    if (m_eventReaderTid) {
        sensordLogD() << "Canceling event reader thread";
        int err = pthread_cancel(m_eventReaderTid);
        if (err) {
            sensordLogC() << "Failed to cancel event reader thread";
        } else {
            sensordLogD() << "Waiting for event reader thread to exit";
            void *ret = 0;
            struct timespec tmo = { 0, 0};
            clock_gettime(CLOCK_REALTIME, &tmo);
            tmo.tv_sec += 3;
            err = pthread_timedjoin_np(m_eventReaderTid, &ret, &tmo);
            if (err) {
                sensordLogC() << "Event reader thread did not exit";
            } else {
                sensordLogD() << "Event reader thread terminated";
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

#ifdef USE_BINDER
    gbinder_fmq_unref(m_wakeLockQueue);
    m_wakeLockQueue = NULL;

    gbinder_fmq_unref(m_eventQueue);
    m_eventQueue = NULL;

    gbinder_client_unref(m_client);
    m_client = NULL;

    gbinder_servicemanager_unref(m_serviceManager);
    m_serviceManager = NULL;
    m_remote = NULL; // auto-release

    for (int i = 0 ; i < m_sensorCount ; i++) {
        g_free((void*)m_sensorArray[i].name.data.str);
        g_free((void*)m_sensorArray[i].vendor.data.str);
        g_free((void*)m_sensorArray[i].typeAsString.data.str);
        g_free((void*)m_sensorArray[i].requiredPermission.data.str);
    }
    delete[] m_sensorArray;
    m_sensorArray = NULL;
#else
    if (m_halDevice) {
        sensordLogD() << "close sensor device";
        int errorCode = sensors_close_1(m_halDevice);
        if (errorCode != 0) {
            sensordLogW() << "sensors_close() failed:" << strerror(-errorCode);
        }
        m_halDevice = NULL;
    }
#endif

    delete[] m_sensorState;
    m_sensorState = NULL;
    m_sensorCount = 0;
    m_initialized = false;

    /* Close sensor data transfer pipe */
    cleanupEventPipe();
}

HybrisManager *HybrisManager::instance()
{
    HybrisManager *priv = hybrisManager();
    return priv;
}

#ifdef USE_BINDER

GBinderLocalReply *HybrisManager::sensorCallbackHandler(
    GBinderLocalObject* obj,
    GBinderRemoteRequest* req,
    guint code,
    guint flags,
    int* status,
    void* user_data)
{
    (void)flags;
    (void)obj;
    (void)user_data;
    sensordLogD() << "sensorCallbackHandler";
    const char *iface = gbinder_remote_request_interface(req);
    if (iface && (!strcmp(iface, SENSOR_BINDER_SERVICE_IFACE_2_0) ||
                  !strcmp(iface, SENSOR_BINDER_SERVICE_IFACE_2_1)
                  )) {
        switch (code) {
        case DYNAMIC_SENSORS_CONNECTED_2_0:
        case DYNAMIC_SENSORS_CONNECTED_2_1:
            sensordLogD() << "Dynamic sensor connected";
            break;
        case DYNAMIC_SENSORS_DISCONNECTED_2_0:
            sensordLogD() << "Dynamic sensor disconnected";
            break;
        default:
            sensordLogW() << "Unknown code (" << code << ")";
            break;
        }
        *status = GBINDER_STATUS_OK;
        sensordLogD() << "sensorCallbackHandler valid sensor interface";
    }
    return NULL;
}

void HybrisManager::getSensorList()
{
    sensordLogD() << "Get sensor list";
    GBinderReader reader;
    GBinderRemoteReply *reply;
    int status;

    if (m_sensorInterfaceEnum == SENSOR_INTERFACE_2_1) {
        reply = gbinder_client_transact_sync_reply(m_client, GET_SENSORS_LIST_2_1, NULL, &status);
    } else {
        reply = gbinder_client_transact_sync_reply(m_client, GET_SENSORS_LIST, NULL, &status);
    }

    if (status != GBINDER_STATUS_OK) {
        sensordLogW() << "Unable to get sensor list: status " << status;
        cleanup();
        sleep(1);
        startConnect();
        return;
    }

    gbinder_remote_reply_init_reader(reply, &reader);
    gbinder_reader_read_int32(&reader, &status);
    gsize count = 0;
    gsize vecSize = 0;
    sensor_t *vec = (sensor_t *)gbinder_reader_read_hidl_vec(&reader, &count, &vecSize);

    m_sensorCount = count;
    m_sensorArray = new sensor_t[m_sensorCount];
    for (int i = 0 ; i < m_sensorCount ; i++) {
        memcpy(&m_sensorArray[i], &vec[i], sizeof(sensor_t));

        // Read strings
        GBinderBuffer *buffer = gbinder_reader_read_buffer(&reader);
        m_sensorArray[i].name.data.str = g_strdup((const gchar *)buffer->data);
        m_sensorArray[i].name.len = buffer->size;
        m_sensorArray[i].name.owns_buffer = true;
        gbinder_buffer_free(buffer);

        buffer = gbinder_reader_read_buffer(&reader);
        m_sensorArray[i].vendor.data.str = g_strdup((const gchar *)buffer->data);
        m_sensorArray[i].vendor.len = buffer->size;
        m_sensorArray[i].vendor.owns_buffer = true;
        gbinder_buffer_free(buffer);

        buffer = gbinder_reader_read_buffer(&reader);
        m_sensorArray[i].typeAsString.data.str = g_strdup((const gchar *)buffer->data);
        m_sensorArray[i].typeAsString.len = buffer->size;
        m_sensorArray[i].typeAsString.owns_buffer = true;
        gbinder_buffer_free(buffer);

        buffer = gbinder_reader_read_buffer(&reader);
        m_sensorArray[i].requiredPermission.data.str = g_strdup((const gchar *)buffer->data);
        m_sensorArray[i].requiredPermission.len = buffer->size;
        m_sensorArray[i].requiredPermission.owns_buffer = true;
        gbinder_buffer_free(buffer);
    }
    gbinder_remote_reply_unref(reply);

    initManager();

    m_initialized = true;
    sensordLogW() << "Hybris sensor manager initialized";
}

void HybrisManager::binderDied(GBinderRemoteObject *, void *user_data)
{
    HybrisManager *conn =
                    static_cast<HybrisManager *>(user_data);
    sensordLogW() << "Sensor service died! Trying to reconnect.";
    conn->cleanup();
    conn->startConnect();
}

void HybrisManager::startConnect()
{
    if (!m_serviceManager) {
        m_serviceManager = gbinder_servicemanager_new(SENSOR_BINDER_SERVICE_DEVICE);
    }

    if (gbinder_servicemanager_wait(m_serviceManager, -1)) {
        finishConnect();
    } else {
        sensordLogW() << "Could not get service manager for sensor service";
        cleanup();
    }
}

void HybrisManager::finishConnect()
{
    int initializeCode;
    m_remote = gbinder_servicemanager_get_service_sync(m_serviceManager,
                                    SENSOR_BINDER_SERVICE_NAME_2_1, NULL);

    if (m_remote) {
        sensordLogD() << "Connected to sensor 2.1 service";
        m_sensorInterfaceEnum = SENSOR_INTERFACE_2_1;
        initializeCode = INITIALIZE_2_1;
    } else {
        m_remote = gbinder_servicemanager_get_service_sync(m_serviceManager,
                                    SENSOR_BINDER_SERVICE_NAME_2_0, NULL);
        if (m_remote) {
            sensordLogD() << "Connected to sensor 2.0 service";
            m_sensorInterfaceEnum = SENSOR_INTERFACE_2_0;
            initializeCode = INITIALIZE_2_0;
        }
    }

    if (m_remote) {
        sensordLogD() << "Initialize sensor service";
        m_deathId = gbinder_remote_object_add_death_handler(m_remote, binderDied, this);
        m_client = gbinder_client_new2(m_remote, sensors_2_client_ifaces, G_N_ELEMENTS(sensors_2_client_ifaces));
        if (!m_client) {
            sensordLogD() << "Could not create client for sensor service. Trying to reconnect.";
        } else {
            GBinderRemoteReply *reply;
            GBinderLocalRequest *req = gbinder_client_new_request2(m_client, initializeCode);
            int32_t status;
            GBinderWriter writer;

            gbinder_local_request_init_writer(req, &writer);
            m_sensorCallback = gbinder_servicemanager_new_local_object2(
                m_serviceManager,
                sensors_2_callback_ifaces,
                sensorCallbackHandler,
                this);

            m_eventQueue = gbinder_fmq_new(sizeof(sensors_event_t), 128,
                GBINDER_FMQ_TYPE_SYNC_READ_WRITE, GBINDER_FMQ_FLAG_CONFIGURE_EVENT_FLAG, -1, 0);
            gbinder_writer_append_fmq_descriptor(&writer, m_eventQueue);

            m_wakeLockQueue = gbinder_fmq_new(sizeof(guint32), 128,
                GBINDER_FMQ_TYPE_SYNC_READ_WRITE, GBINDER_FMQ_FLAG_CONFIGURE_EVENT_FLAG, -1, 0);
            gbinder_writer_append_fmq_descriptor(&writer, m_wakeLockQueue);

            gbinder_writer_append_local_object(&writer, m_sensorCallback);

            reply = gbinder_client_transact_sync_reply(m_client, initializeCode, req, &status);
            gbinder_local_request_unref(req);

            if (status != GBINDER_STATUS_OK) {
                sensordLogW() << "Initialize failed with status" << status << ". Trying to reconnect.";
                gbinder_remote_reply_unref(reply);
            } else {
                int error;
                GBinderReader reader;
                gbinder_remote_reply_init_reader(reply, &reader);
                gbinder_reader_read_int32(&reader, &status);
                gbinder_reader_read_int32(&reader, &error);

                gbinder_remote_reply_unref(reply);
                if (!error) {
                    getSensorList();
                    return;
                } else {
                    sensordLogW() << "Initialize failed with error" << error << ". Trying to reconnect.";
                }
            }
        }
    } else {
        m_remote = gbinder_servicemanager_get_service_sync(m_serviceManager,
                                    SENSOR_BINDER_SERVICE_NAME_1_0, NULL);
        if (!m_remote) {
            sensordLogD() << "Could not find remote object for sensor service. Trying to reconnect";
        } else {
            m_sensorInterfaceEnum = SENSOR_INTERFACE_1_0;
            sensordLogD() << "Connected to sensor 1.0 service";
            m_deathId = gbinder_remote_object_add_death_handler(m_remote, binderDied,
                            this);
            m_client = gbinder_client_new(m_remote, SENSOR_BINDER_SERVICE_IFACE_1_0);
            if (!m_client) {
                sensordLogD() << "Could not create client for sensor service. Trying to reconnect.";
            } else {
                // Sometimes sensor service has lingering connetion from
                // previous client which causes sensor service to restart
                // and we need to test with poll if remote is really working.
                GBinderRemoteReply *reply;
                GBinderLocalRequest *req = gbinder_client_new_request2(m_client, POLL);
                int32_t status;

                // Empty poll to test if remote is working
                req = gbinder_local_request_append_int32(req, 0);

                reply = gbinder_client_transact_sync_reply(m_client, POLL, req, &status);
                gbinder_local_request_unref(req);
                gbinder_remote_reply_unref(reply);

                if (status != GBINDER_STATUS_OK) {
                    sensordLogW() << "Poll failed with status" << status << ". Trying to reconnect.";
                } else {
                    getSensorList();
                    return;
                }
            }
        }
    }
    // On failure cleanup and wait before reconnecting
    cleanup();
    sleep(1);
    startConnect();
}
#endif //USE_BINDER

int HybrisManager::handleForType(int sensorType) const
{
    int index = indexForType(sensorType);
    return (index < 0) ? -1 : m_sensorArray[index].handle;
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
        sensordLogW("HYBRIS CTL invalid sensor handle: %d", handle);
    return index;
}

int HybrisManager::indexForType(int sensorType) const
{
    int index = m_indexOfType.value(sensorType, -1);
    if (index == -1)
        sensordLogW("HYBRIS CTL invalid sensor type: %d", sensorType);
    return index;
}

void HybrisManager::startReader(HybrisAdaptor *adaptor)
{
    if (m_registeredAdaptors.values().contains(adaptor)) {
        sensordLogD() << "activating " << adaptor->name() << adaptor->m_sensorHandle;
        if (!setActive(adaptor->m_sensorHandle, true)) {
            sensordLogW() <<Q_FUNC_INFO<< "failed";
            adaptor->setValid(false);
        }
    }
}

void HybrisManager::stopReader(HybrisAdaptor *adaptor)
{
    if (m_registeredAdaptors.values().contains(adaptor)) {
            sensordLogD() << "deactivating " << adaptor->name();
            if (!setActive(adaptor->m_sensorHandle, false)) {
                sensordLogW() <<Q_FUNC_INFO<< "failed";
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
        const struct sensor_t *sensor = &m_sensorArray[index];

        range = scaleSensorValue(sensor->maxRange, sensor->type);
        sensordLogT("HYBRIS CTL getMaxRange(%d=%s) -> %g",
                    sensor->handle, sensorTypeName(sensor->type), range);
    }

    return range;
}

float HybrisManager::getResolution(int handle) const
{
    float resolution = 0;
    int index = indexForHandle(handle);

    if (index != -1) {
        const struct sensor_t *sensor = &m_sensorArray[index];

        resolution = scaleSensorValue(sensor->resolution, sensor->type);
        sensordLogT("HYBRIS CTL getResolution(%d=%s) -> %g",
                    sensor->handle, sensorTypeName(sensor->type), resolution);
    }

    return resolution;
}

int HybrisManager::getMinDelay(int handle) const
{
    int delay_us = 0;
    int index = indexForHandle(handle);

    if (index != -1) {
        const struct sensor_t *sensor = &m_sensorArray[index];
        HybrisSensorState     *state  = &m_sensorState[index];

        delay_us = state->m_minDelay_us;
        sensordLogT("HYBRIS CTL getMinDelay(%d=%s) -> %d",
                    sensor->handle, sensorTypeName(sensor->type), delay_us);
    }

    return delay_us;
}

int HybrisManager::getMaxDelay(int handle) const
{
    int delay_us = 0;
    int index = indexForHandle(handle);

    if (index != -1) {
        const struct sensor_t *sensor = &m_sensorArray[index];
        HybrisSensorState     *state  = &m_sensorState[index];

        delay_us = state->m_maxDelay_us;
        sensordLogT("HYBRIS CTL getMaxDelay(%d=%s) -> %d",
                    sensor->handle, sensorTypeName(sensor->type), delay_us);
    }

    return delay_us;
}

int HybrisManager::getDelay(int handle) const
{
    int delay_us = 0;
    int index = indexForHandle(handle);

    if (index != -1) {
        const struct sensor_t *sensor = &m_sensorArray[index];
        HybrisSensorState     *state  = &m_sensorState[index];

        delay_us = state->m_delay_us;
        sensordLogT("HYBRIS CTL getDelay(%d=%s) -> %d",
                    sensor->handle, sensorTypeName(sensor->type), delay_us);
    }

    return delay_us;
}

bool HybrisManager::setDelay(int handle, int delay_us, bool force)
{
    bool success = false;
    int index = indexForHandle(handle);

    if (index != -1) {
        const struct sensor_t *sensor = &m_sensorArray[index];
        HybrisSensorState     *state  = &m_sensorState[index];

        if (!force && state->m_delay_us == delay_us) {
            sensordLogT("HYBRIS CTL setDelay(%d=%s, %d) -> no-change",
                        sensor->handle, sensorTypeName(sensor->type), delay_us);
            success = true;
        } else {
            int64_t delay_ns = delay_us * 1000LL;
#ifdef USE_BINDER
            int error;
            GBinderLocalRequest *req = gbinder_client_new_request2(m_client, BATCH);
            GBinderRemoteReply *reply;
            GBinderReader reader;
            GBinderWriter writer;
            int32_t status;

            gbinder_local_request_init_writer(req, &writer);

            gbinder_writer_append_int32(&writer, sensor->handle);
            gbinder_writer_append_int64(&writer, delay_ns);
            gbinder_writer_append_int64(&writer, 0);

            reply = gbinder_client_transact_sync_reply(m_client, BATCH, req, &status);
            gbinder_local_request_unref(req);

            if (status != GBINDER_STATUS_OK) {
                sensordLogW() << "Set delay failed status " << status;
                return false;
            }
            gbinder_remote_reply_init_reader(reply, &reader);
            gbinder_reader_read_int32(&reader, &status);
            gbinder_reader_read_int32(&reader, &error);

            gbinder_remote_reply_unref(reply);
#else
            int error;
            if (m_halDevice->common.version >= SENSORS_DEVICE_API_VERSION_1_0)
                error = m_halDevice->batch(m_halDevice, sensor->handle, 0, delay_ns, 0);
            else
                error = m_halDevice->setDelay(&m_halDevice->v0, sensor->handle, delay_ns);
#endif
            if (error) {
                sensordLogW("HYBRIS CTL setDelay(%d=%s, %d) -> %d=%s",
                            sensor->handle, sensorTypeName(sensor->type), delay_us,
                            error, strerror(error));
            } else {
                sensordLogD("HYBRIS CTL setDelay(%d=%s, %d) -> success",
                            sensor->handle, sensorTypeName(sensor->type), delay_us);
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
        const struct sensor_t *sensor = &m_sensorArray[index];
        HybrisSensorState     *state  = &m_sensorState[index];

        active = (state->m_active > 0);
        sensordLogT("HYBRIS CTL getActive(%d=%s) -> %s",
                    sensor->handle, sensorTypeName(sensor->type),
                    active ? "true" : "false");
    }
    return active;
}

bool HybrisManager::setActive(int handle, bool active)
{
    bool success = false;
    int index = indexForHandle(handle);

    if (index != -1) {
        const struct sensor_t *sensor = &m_sensorArray[index];
        HybrisSensorState     *state  = &m_sensorState[index];

        if (state->m_active == active) {
            sensordLogT("HYBRIS CTL setActive(%d=%s, %s) -> no-change",
                        sensor->handle, sensorTypeName(sensor->type), active ? "true" : "false");
            success = true;
        } else {
#ifdef USE_BINDER
            if (active && state->m_delay_us != -1) {
                sensordLogD("HYBRIS CTL FORCE PRE UPDATE %i, %s", sensor->handle, sensorTypeName(sensor->type));
                int delay_us = state->m_delay_us;
                state->m_delay_us = -1;
                setDelay(handle, delay_us, true);
            }
            int error;
            GBinderLocalRequest *req = gbinder_client_new_request2(m_client, ACTIVATE);
            GBinderRemoteReply *reply;
            GBinderReader reader;
            GBinderWriter writer;
            int32_t status;

            gbinder_local_request_init_writer(req, &writer);

            gbinder_writer_append_int32(&writer, sensor->handle);
            gbinder_writer_append_int32(&writer, active);

            reply = gbinder_client_transact_sync_reply(m_client, ACTIVATE, req, &status);
            gbinder_local_request_unref(req);

            if (status != GBINDER_STATUS_OK) {
                sensordLogW() << "Activate failed status " << status;
                return false;
            }
            gbinder_remote_reply_init_reader(reply, &reader);
            gbinder_reader_read_int32(&reader, &status);
            gbinder_reader_read_int32(&reader, &error);

            gbinder_remote_reply_unref(reply);
#else
            int error = m_halDevice->activate(&m_halDevice->v0, sensor->handle, active);
#endif
            if (error) {
                sensordLogW("HYBRIS CTL setActive%d=%s, %s) -> %d=%s",
                            sensor->handle, sensorTypeName(sensor->type), active ? "true" : "false",
                            error, strerror(error));
            } else {
                sensordLogD("HYBRIS CTL setActive%d=%s, %s) -> success",
                            sensor->handle, sensorTypeName(sensor->type), active ? "true" : "false");
                state->m_active = active;
                success = true;
            }
#ifndef USE_BINDER
            if (state->m_active == true && state->m_delay_us != -1) {
                sensordLogD("HYBRIS CTL FORCE DELAY UPDATE");
                int delay_us = state->m_delay_us;
                state->m_delay_us = -1;
                setDelay(handle, delay_us, false);
            }
#endif
        }
    }
    return success;
}

#ifdef USE_BINDER
/**
 * pollEvents is only called during initialization and after that from pollEventsCallback
 * triggered by binder reply so there is only maximum of one active poll at all times
 */
void HybrisManager::pollEvents()
{
    if (m_client) {
        GBinderLocalRequest *req = gbinder_client_new_request2(m_client, POLL);

        req = gbinder_local_request_append_int32(req, 16); // Same number as for HAL
        m_pollTransactId = gbinder_client_transact(m_client, POLL, 0, req, pollEventsCallback, 0, this);
        gbinder_local_request_unref(req);
    }
}

void HybrisManager::pollEventsCallback(
    GBinderClient* /*client*/,
    GBinderRemoteReply* reply,
    int status,
    void* userData)
{
    HybrisManager *manager = static_cast<HybrisManager *>(userData);
    GBinderReader reader;
    int32_t readerStatus;
    int32_t result;
    sensors_event_t *buffer;

    manager->m_pollTransactId = 0;

    if (status != GBINDER_STATUS_OK) {
        sensordLogW() << "Poll failed status " << status;
        // In case of binder failure sleep a little before attempting a new poll
        struct timespec ts = { 0, 50 * 1000 * 1000 }; // 50 ms
        do { } while (nanosleep(&ts, &ts) == -1 && errno == EINTR);
    } else {
        // Read sensor events from reply
        gbinder_remote_reply_init_reader(reply, &reader);
        gbinder_reader_read_int32(&reader, &readerStatus);
        gbinder_reader_read_int32(&reader, &result);
        gsize structSize = 0;
        gsize eventCount = 0;

        buffer = (sensors_event_t *)gbinder_reader_read_hidl_vec(&reader, &eventCount , &structSize);
        manager->queueEvents(buffer, eventCount);
    }
    // Initiate new poll
    manager->pollEvents();
}
#endif

void *HybrisManager::eventReaderThread(void *aptr)
{
    HybrisManager *manager = static_cast<HybrisManager *>(aptr);
    sensors_event_t buffer[maxEvents];

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
#ifdef USE_BINDER
        size_t availableEvents = gbinder_fmq_available_to_read(manager->m_eventQueue);

        if (availableEvents <= 0) {
            guint32 state = 0;
            /* Async cancellation point */
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
            gint32 ret = gbinder_fmq_wait(manager->m_eventQueue,
                EVENT_QUEUE_FLAG_READ_AND_PROCESS, &state);
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);

            if (ret < 0) {
                if (ret != -ETIMEDOUT && ret != -EAGAIN) {
                    sensordLogW() << "Waiting for events failed" << strerror(-ret);
                }
                continue;
            }
            availableEvents = gbinder_fmq_available_to_read(manager->m_eventQueue);
        }
        size_t numEvents = std::min(availableEvents, maxEvents);
        if (numEvents == 0) {
            continue;
        }
        if (gbinder_fmq_read(manager->m_eventQueue, buffer, numEvents)) {
            gbinder_fmq_wake(manager->m_eventQueue, EVENT_QUEUE_FLAG_EVENTS_READ);
        } else {
            sensordLogW() << "Reading events failed";
            continue;
        }

        // Queue received events for processing in main thread
        int wakeupEventCount = manager->queueEvents(buffer, numEvents);

        // Acknowledge wakeup events
        if (wakeupEventCount) {
            if (gbinder_fmq_write(manager->m_wakeLockQueue, &wakeupEventCount, 1)) {
                gbinder_fmq_wake(manager->m_wakeLockQueue, WAKE_LOCK_QUEUE_DATA_WRITTEN);
            } else {
                sensordLogW() << "Write to wakelock queue failed";
            }
        }
#else // HAL reader
        /* Async cancellation point at android hal poll() */
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
        int numEvents = manager->m_halDevice->poll(&manager->m_halDevice->v0, buffer, maxEvents);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
        /* Rate limit in poll() error situations */
        if (numEvents < 0) {
            sensordLogW() << "android device->poll() failed" << strerror(-numEvents);
            struct timespec ts = { 1, 0 }; // 1000 ms
            do { } while (nanosleep(&ts, &ts) == -1 && errno == EINTR);
            continue;
        }
        // Queue received events for processing in main thread
        manager->queueEvents(buffer, numEvents);
#endif
    }
    return 0;
}

void HybrisManager::initEventPipe()
{
    sensordLogD("initialize event pipe");
    int pfd[2] = {-1, -1};
    if (::pipe2(pfd, O_CLOEXEC) == -1) {
        sensordLogW("failed to create event pipe: %s", strerror(errno));
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
    sensordLogD("cleanup event pipe");
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
        sensordLogW("fd mismatch, event pipe notifier disabled");
    } else {
        sensors_event_t events[maxEvents];
        ssize_t rc = ::read(m_eventPipeReadFd, events, sizeof events);
        if (rc == 0) {
            sensordLogW("event pipe eof, notifier disabled");
            m_eventPipeNotifier->setEnabled(false);
        } else if (rc == -1) {
            if (errno != EAGAIN && errno != EINTR) {
                sensordLogW("event pipe %s, notifier disabled", strerror(errno));
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
        sensordLogT("QUEUE HYBRIS EVE %s", sensorTypeName(data.type));
#ifdef USE_BINDER
        int index = indexForHandle(data.sensor);
        const struct sensor_t *sensor = &m_sensorArray[index];
        if (sensor->flags & SENSOR_FLAG_WAKE_UP) {
            ++wakeupEventCount;
        }
#else
        if (data.version != sizeof(sensors_event_t)) {
            sensordLogW()<< QString("incorrect event version (version=%1, expected=%2").arg(data.version).arg(sizeof(sensors_event_t));
            errorInInput = true;
        }
        if (data.type == SENSOR_TYPE_PROXIMITY) {
            ++wakeupEventCount;
        }
#endif
    }

    /* Suspend proof sensor data forwarding */
    if (wakeupEventCount)
        ObtainTemporaryWakeLock();

    /* Forward via pipe for processing in main threah */
    if (!errorInInput && numEvents > 0 && m_eventPipeWriteFd != -1) {
        if (::write(m_eventPipeWriteFd, buffer, numEvents * sizeof *buffer) == -1) {
            sensordLogW("event pipe write failure: %s", strerror(errno));
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
#ifdef USE_BINDER
        int index = indexForHandle(data.sensor);
        const struct sensor_t *sensor = &m_sensorArray[index];
        if (sensor->flags & SENSOR_FLAG_WAKE_UP) {
            ++wakeupEventCount;
        }
#else
        if (data.type == SENSOR_TYPE_PROXIMITY) {
            ++wakeupEventCount;
        }
#endif
    }

    /* Suspend proof sensor data processing */
    if (wakeupEventCount)
        ObtainTemporaryWakeLock();

    /* Push the sensor data down chains */
    for (int i = 0; i < numEvents; i++) {
        const sensors_event_t& data = buffer[i];
        sensordLogT("HYBRIS EVE %s", sensorTypeName(data.type));

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
        sensordLogW() << Q_FUNC_INFO <<"no such sensor" << id;
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
    sensordLogT() << "Writing to '" << path << ": " << content;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        sensordLogW() << "Failed to open '" << path << "': " << file.errorString();
        return false;
    }
    if (file.write(content.constData(), content.size()) == -1)
    {
        sensordLogW() << "Failed to write to '" << path << "': " << file.errorString();
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
        sensordLogW() << id() << Q_FUNC_INFO << "setInterval not ok";
    } else {
        /* If we have not yet received sensor data, apply fallback value */
        sensors_event_t *fallback = hybrisManager()->eventForHandle(m_sensorHandle);
        if (fallback && fallback->sensor == m_sensorHandle && fallback->type == m_sensorType) {
            sensordLogT("HYBRIS FALLBACK type:%s sensor:%d",
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
    if (entry == NULL) {
        sensordLogW() << id() << Q_FUNC_INFO << "Sensor not found: " << name();
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
                sensordLogT("HYBRIS FALLBACK type:%s sensor:%d",
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
        sensordLogT() << id() << Q_FUNC_INFO << "entry" << entry->name()
                      << "refs:" << entry->referenceCount() << "running:" << entry->isRunning();
    }
}

bool HybrisAdaptor::startSensor()
{
    // Note: This is overloaded and called by each HybrisXxxAdaptor::startSensor()
    if (!m_shouldBeRunning) {
        m_shouldBeRunning = true;
        sensordLogT("%s m_shouldBeRunning = %d", sensorTypeName(m_sensorType), m_shouldBeRunning);
        evaluateSensor();
    }
    return true;
}

void HybrisAdaptor::stopSensor()
{
    // Note: This is overloaded and called by each HybrisXxxAdaptor::stopSensor()
    if (m_shouldBeRunning) {
        m_shouldBeRunning = false;
        sensordLogT("%s m_shouldBeRunning = %d", sensorTypeName(m_sensorType), m_shouldBeRunning);
        evaluateSensor();
    }
}

bool HybrisAdaptor::standby()
{
    if (!m_inStandbyMode) {
        m_inStandbyMode = true;
        sensordLogT("%s m_inStandbyMode = %d", sensorTypeName(m_sensorType), m_inStandbyMode);
        evaluateSensor();
    }
    return true;
}

bool HybrisAdaptor::resume()
{
    if (m_inStandbyMode) {
        m_inStandbyMode = false;
        sensordLogT("%s m_inStandbyMode = %d", sensorTypeName(m_sensorType), m_inStandbyMode);
        evaluateSensor();
    }
    return true;
}
