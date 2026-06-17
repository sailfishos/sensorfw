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

#include "hybrisbackend_binder_hidl.h"

#include "hybrisadaptor.h"
#include "logging.h"

#include <QCoreApplication>

#include <unistd.h>

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
    nullptr
};

extern char const *sensorTypeName(int type);

HybrisBackendBinderHidl::HybrisBackendBinderHidl(HybrisManager *manager)
    : HybrisBackendBinder(manager)
    , m_pollTransactId(0)
{
}

HybrisBackendBinderHidl::~HybrisBackendBinderHidl()
{
    cleanup();
}

bool HybrisBackendBinderHidl::isSupported()
{
    bool ret = false;
    GBinderServiceManager *sm =
        gbinder_servicemanager_new(SENSOR_BINDER_SERVICE_DEVICE);

    if (gbinder_servicemanager_is_present(sm)) {
        /* Fetch remote reference from hwservicemanager */
        GBinderRemoteObject *remote =
            gbinder_servicemanager_get_service_sync(sm,
                SENSOR_BINDER_SERVICE_NAME_2_1, nullptr);
        if (remote) {
            ret = true;
        } else {
            remote =
                gbinder_servicemanager_get_service_sync(sm,
                    SENSOR_BINDER_SERVICE_NAME_2_0, nullptr);
            if (remote) {
                ret = true;
            } else {
                remote =
                    gbinder_servicemanager_get_service_sync(sm,
                        SENSOR_BINDER_SERVICE_NAME_1_0, nullptr);
                if (remote) {
                    ret = true;
                }
            }
        }
    }

    gbinder_servicemanager_unref(sm);

    return ret;
}

void HybrisBackendBinderHidl::cleanup()
{
    qCInfo(lcSensorFw) << "cleanup";
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
    HybrisBackendBinder::cleanup();
}

void HybrisBackendBinderHidl::initialize()
{
    startConnect();
}

bool HybrisBackendBinderHidl::needsReaderThread()
{
    if (m_sensorInterfaceEnum == SENSOR_INTERFACE_1_0) {
        pollEvents();
        return false;
    }
    return true;
}

int HybrisBackendBinderHidl::setActive(int handle, bool active)
{
    if (active && m_manager->getDelay(handle) != -1) {
        int index = m_manager->indexForHandle(handle);
        qCInfo(lcSensorFw, "HYBRIS CTL FORCE PRE UPDATE %i, %s", handle,
               sensorTypeName(m_sensorArray[index].type));
        int delay_us = m_manager->getDelay(handle);
        m_manager->setDelay(handle, delay_us, true);
    }

    int error;
    GBinderLocalRequest *req = gbinder_client_new_request2(m_client, ACTIVATE);
    GBinderRemoteReply *reply;
    GBinderReader reader;
    GBinderWriter writer;
    int32_t status;

    gbinder_local_request_init_writer(req, &writer);

    gbinder_writer_append_int32(&writer, handle);
    gbinder_writer_append_int32(&writer, active);

    reply = gbinder_client_transact_sync_reply(m_client, ACTIVATE, req, &status);
    gbinder_local_request_unref(req);

    if (!reply || status != GBINDER_STATUS_OK) {
        qCWarning(lcSensorFw) << "Activate failed";
        return false;
    }
    gbinder_remote_reply_init_reader(reply, &reader);
    gbinder_reader_read_int32(&reader, &status);
    gbinder_reader_read_int32(&reader, &error);

    gbinder_remote_reply_unref(reply);

    return error;
}

int HybrisBackendBinderHidl::setDelay(int handle, int64_t delay_ns)
{
    int error;
    GBinderLocalRequest *req = gbinder_client_new_request2(m_client, BATCH);
    GBinderRemoteReply *reply;
    GBinderReader reader;
    GBinderWriter writer;
    int32_t status;

    gbinder_local_request_init_writer(req, &writer);

    gbinder_writer_append_int32(&writer, handle);
    gbinder_writer_append_int64(&writer, delay_ns);
    gbinder_writer_append_int64(&writer, 0);

    reply = gbinder_client_transact_sync_reply(m_client, BATCH, req, &status);
    gbinder_local_request_unref(req);

    if (!reply || status != GBINDER_STATUS_OK) {
        qCWarning(lcSensorFw) << "Set delay failed";
        return false;
    }
    gbinder_remote_reply_init_reader(reply, &reader);
    gbinder_reader_read_int32(&reader, &status);
    gbinder_reader_read_int32(&reader, &error);

    gbinder_remote_reply_unref(reply);

    return error;
}

/**
 * pollEvents is only called during initialization and after that from pollEventsCallback
 * triggered by binder reply so there is only maximum of one active poll at all times
 */
void HybrisBackendBinderHidl::pollEvents()
{
    if (m_client) {
        GBinderLocalRequest *req = gbinder_client_new_request2(m_client, POLL);

        req = gbinder_local_request_append_int32(req, 16); // Same number as for HAL
        m_pollTransactId = gbinder_client_transact(m_client, POLL, 0, req, pollEventsCallback, 0, this);
        gbinder_local_request_unref(req);
    }
}

void HybrisBackendBinderHidl::pollEventsCallback(
    GBinderClient* /*client*/,
    GBinderRemoteReply* reply,
    int status,
    void* userData)
{
    HybrisBackendBinderHidl *backend = static_cast<HybrisBackendBinderHidl *>(userData);
    GBinderReader reader;
    int32_t readerStatus;
    int32_t result;
    sensors_event_t *buffer;

    backend->m_pollTransactId = 0;

    if (status != GBINDER_STATUS_OK) {
        qCWarning(lcSensorFw) << "Poll failed status " << status;
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
        backend->m_manager->queueEvents(buffer, eventCount);
    }
    // Initiate new poll
    backend->pollEvents();
}

void HybrisBackendBinderHidl::readEvents()
{
    sensors_event_t buffer[maxEvents];
    size_t availableEvents = gbinder_fmq_available_to_read(m_eventQueue);

    if (availableEvents <= 0) {
        guint32 state = 0;
        /* Async cancellation point */
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
        gint32 ret = gbinder_fmq_wait(m_eventQueue,
            EVENT_QUEUE_FLAG_READ_AND_PROCESS, &state);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);

        if (ret < 0) {
            if (ret != -ETIMEDOUT && ret != -EAGAIN) {
                qCWarning(lcSensorFw) << "Waiting for events failed" << strerror(-ret);
            }
            return;
        }
        availableEvents = gbinder_fmq_available_to_read(m_eventQueue);
    }
    size_t numEvents = std::min(availableEvents, maxEvents);
    if (numEvents == 0) {
        return;
    }
    if (gbinder_fmq_read(m_eventQueue, buffer, numEvents)) {
        gbinder_fmq_wake(m_eventQueue, EVENT_QUEUE_FLAG_EVENTS_READ);
    } else {
        qCWarning(lcSensorFw) << "Reading events failed";
        return;
    }

    // Queue received events for processing in main thread
    int wakeupEventCount = m_manager->queueEvents(buffer, numEvents);

    // Acknowledge wakeup events
    if (wakeupEventCount) {
        if (gbinder_fmq_write(m_wakeLockQueue, &wakeupEventCount, 1)) {
            gbinder_fmq_wake(m_wakeLockQueue, WAKE_LOCK_QUEUE_DATA_WRITTEN);
        } else {
            qCWarning(lcSensorFw) << "Write to wakelock queue failed";
        }
    }
}

GBinderLocalReply *HybrisBackendBinderHidl::sensorCallbackHandler(
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
    qCInfo(lcSensorFw) << "sensorCallbackHandler";
    const char *iface = gbinder_remote_request_interface(req);
    if (iface && (!strcmp(iface, SENSOR_BINDER_SERVICE_IFACE_2_0) ||
                  !strcmp(iface, SENSOR_BINDER_SERVICE_IFACE_2_1)
                  )) {
        switch (code) {
        case DYNAMIC_SENSORS_CONNECTED_2_0:
        case DYNAMIC_SENSORS_CONNECTED_2_1:
            qCInfo(lcSensorFw) << "Dynamic sensor connected";
            break;
        case DYNAMIC_SENSORS_DISCONNECTED_2_0:
            qCInfo(lcSensorFw) << "Dynamic sensor disconnected";
            break;
        default:
            qCWarning(lcSensorFw) << "Unknown code (" << code << ")";
            break;
        }
        *status = GBINDER_STATUS_OK;
        qCInfo(lcSensorFw) << "sensorCallbackHandler valid sensor interface";
    }
    return nullptr;
}

void HybrisBackendBinderHidl::getSensorList()
{
    qCInfo(lcSensorFw) << "Get sensor list";
    GBinderReader reader;
    GBinderRemoteReply *reply;
    int status;

    if (m_sensorInterfaceEnum == SENSOR_INTERFACE_2_1) {
        reply = gbinder_client_transact_sync_reply(m_client, GET_SENSORS_LIST_2_1, nullptr, &status);
    } else {
        reply = gbinder_client_transact_sync_reply(m_client, GET_SENSORS_LIST, nullptr, &status);
    }

    if (!reply || status != GBINDER_STATUS_OK) {
        qCWarning(lcSensorFw) << "Unable to get sensor list";
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

    m_manager->initManager();

    qCWarning(lcSensorFw) << "Hybris sensor manager initialized";
}

void HybrisBackendBinderHidl::binderDied(GBinderRemoteObject *, void *)
{
    qCWarning(lcSensorFw) << "Sensor service died! Restart sensorfw.";
    // Sensor connections don't recover correctly without restart at the moment
    QCoreApplication::exit(EXIT_FAILURE);
}

void HybrisBackendBinderHidl::startConnect()
{
    if (!m_serviceManager) {
        m_serviceManager = gbinder_servicemanager_new(SENSOR_BINDER_SERVICE_DEVICE);
    }

    if (gbinder_servicemanager_wait(m_serviceManager, -1)) {
        finishConnect();
    } else {
        qCWarning(lcSensorFw) << "Could not get service manager for sensor service";
        cleanup();
    }
}

void HybrisBackendBinderHidl::finishConnect()
{
    int initializeCode;
    m_remote = gbinder_servicemanager_get_service_sync(m_serviceManager,
                                    SENSOR_BINDER_SERVICE_NAME_2_1, nullptr);

    if (m_remote) {
        qCInfo(lcSensorFw) << "Connected to sensor 2.1 service";
        m_sensorInterfaceEnum = SENSOR_INTERFACE_2_1;
        initializeCode = INITIALIZE_2_1;
    } else {
        m_remote = gbinder_servicemanager_get_service_sync(m_serviceManager,
                                    SENSOR_BINDER_SERVICE_NAME_2_0, nullptr);
        if (m_remote) {
            qCInfo(lcSensorFw) << "Connected to sensor 2.0 service";
            m_sensorInterfaceEnum = SENSOR_INTERFACE_2_0;
            initializeCode = INITIALIZE_2_0;
        }
    }

    if (m_remote) {
        qCInfo(lcSensorFw) << "Initialize sensor service";
        m_deathId = gbinder_remote_object_add_death_handler(m_remote, binderDied, this);
        m_client = gbinder_client_new2(m_remote, sensors_2_client_ifaces, G_N_ELEMENTS(sensors_2_client_ifaces));
        if (!m_client) {
            qCInfo(lcSensorFw) << "Could not create client for sensor service. Trying to reconnect.";
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

            if (!reply || status != GBINDER_STATUS_OK) {
                qCWarning(lcSensorFw) << "Initialize failed. Trying to reconnect.";
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
                    qCWarning(lcSensorFw) << "Initialize failed with error" << error << ". Trying to reconnect.";
                }
            }
        }
    } else {
        m_remote = gbinder_servicemanager_get_service_sync(m_serviceManager,
                                    SENSOR_BINDER_SERVICE_NAME_1_0, nullptr);
        if (!m_remote) {
            qCInfo(lcSensorFw) << "Could not find remote object for sensor service. Trying to reconnect";
        } else {
            m_sensorInterfaceEnum = SENSOR_INTERFACE_1_0;
            qCInfo(lcSensorFw) << "Connected to sensor 1.0 service";
            m_deathId = gbinder_remote_object_add_death_handler(m_remote, binderDied,
                            this);
            m_client = gbinder_client_new(m_remote, SENSOR_BINDER_SERVICE_IFACE_1_0);
            if (!m_client) {
                qCInfo(lcSensorFw) << "Could not create client for sensor service. Trying to reconnect.";
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

                if (!reply || status != GBINDER_STATUS_OK) {
                    gbinder_remote_reply_unref(reply);
                    qCWarning(lcSensorFw) << "Poll failed. Trying to reconnect.";
                } else {
                    gbinder_remote_reply_unref(reply);
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
