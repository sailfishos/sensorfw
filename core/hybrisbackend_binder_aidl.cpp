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

#include "hybrisbackend_binder_aidl.h"

#include "hybrisadaptor.h"
#include "logging.h"

#include <QCoreApplication>

#include <unistd.h>

#define SENSOR_BINDER_SERVICE_DEVICE "/dev/binder"
#define SENSOR_BINDER_SERVICE_IFACE_AIDL "android.hardware.sensors.ISensors"
#define SENSOR_BINDER_SERVICE_NAME_AIDL SENSOR_BINDER_SERVICE_IFACE_AIDL "/default"
#define SENSOR_BINDER_SERVICE_CALLBACK_IFACE_AIDL "android.hardware.sensors.ISensorsCallback"

#define MAX_RECEIVE_BUFFER_EVENT_COUNT 128

extern char const *sensorTypeName(int type);

HybrisBackendBinderAidl::HybrisBackendBinderAidl(HybrisManager *manager)
    : HybrisBackendBinder(manager)
{
}

HybrisBackendBinderAidl::~HybrisBackendBinderAidl()
{
    cleanup();
}

bool HybrisBackendBinderAidl::isSupported()
{
    bool ret = false;
    GBinderServiceManager *sm =
        gbinder_servicemanager_new(SENSOR_BINDER_SERVICE_DEVICE);

    if (gbinder_servicemanager_is_present(sm)) {
        /* Fetch remote reference from hwservicemanager */
        GBinderRemoteObject *remote =
            gbinder_servicemanager_get_service_sync(sm,
                SENSOR_BINDER_SERVICE_NAME_AIDL, nullptr);
        if (remote) {
            ret = true;
        }
    }

    gbinder_servicemanager_unref(sm);

    return ret;
}

void HybrisBackendBinderAidl::cleanup()
{
    qCInfo(lcSensorFw) << "cleanup";
    HybrisBackendBinder::cleanup();
}

void HybrisBackendBinderAidl::initialize()
{
    startConnect();
}

bool HybrisBackendBinderAidl::needsReaderThread()
{
    return true;
}

int HybrisBackendBinderAidl::setActive(int handle, bool active)
{
    if (active && m_manager->getDelay(handle) != -1) {
        int index = m_manager->indexForHandle(handle);
        qCInfo(lcSensorFw, "HYBRIS CTL FORCE PRE UPDATE %i, %s", handle,
               sensorTypeName(m_sensorArray[index].type));
        int delay_us = m_manager->getDelay(handle);
        m_manager->setDelay(handle, delay_us, true);
    }

    int error = 0;
    GBinderLocalRequest *req = gbinder_client_new_request(m_client);
    GBinderRemoteReply *reply;
    GBinderReader reader;
    GBinderWriter writer;
    int32_t status;

    gbinder_local_request_init_writer(req, &writer);

    gbinder_writer_append_int32(&writer, handle);
    gbinder_writer_append_int32(&writer, active);

    reply = gbinder_client_transact_sync_reply(m_client, ACTIVATE_AIDL, req, &status);
    gbinder_local_request_unref(req);

    if (!reply || status != GBINDER_STATUS_OK) {
        qCWarning(lcSensorFw) << "Activate failed";
        return false;
    }
    gbinder_remote_reply_init_reader(reply, &reader);
    gbinder_reader_read_int32(&reader, &status);
    if (status) {
        error = status;
    }

    gbinder_remote_reply_unref(reply);

    return error;
}

int HybrisBackendBinderAidl::setDelay(int handle, int64_t delay_ns)
{
    int error = 0;
    GBinderLocalRequest *req = gbinder_client_new_request2(m_client, BATCH);
    GBinderRemoteReply *reply;
    GBinderReader reader;
    GBinderWriter writer;
    int32_t status;

    gbinder_local_request_init_writer(req, &writer);

    gbinder_writer_append_int32(&writer, handle);
    gbinder_writer_append_int64(&writer, delay_ns);
    gbinder_writer_append_int64(&writer, 0);

    reply = gbinder_client_transact_sync_reply(m_client, BATCH_AIDL, req, &status);
    gbinder_local_request_unref(req);

    if (!reply || status != GBINDER_STATUS_OK) {
        qCWarning(lcSensorFw) << "Set delay failed";
        return false;
    }
    gbinder_remote_reply_init_reader(reply, &reader);
    gbinder_reader_read_int32(&reader, &status);
    if (status) {
        error = status;
    }

    gbinder_remote_reply_unref(reply);

    return error;
}

void HybrisBackendBinderAidl::readEvents()
{
    sensors_event_t buffer[maxEvents];
    sensors_event_t_aidl aidl_buffer[maxEvents];
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
    if (gbinder_fmq_read(m_eventQueue, aidl_buffer, numEvents)) {
        gbinder_fmq_wake(m_eventQueue, EVENT_QUEUE_FLAG_EVENTS_READ);
    } else {
        qCWarning(lcSensorFw) << "Reading events failed";
        return;
    }

    for (unsigned int i = 0; i < numEvents; ++i) {
        buffer[i].timestamp = aidl_buffer[i].timestamp;
        buffer[i].sensor = aidl_buffer[i].sensor;
        buffer[i].type = aidl_buffer[i].type;
        // AIDL struct is too big so copy manually when needed
        if (buffer[i].type == SENSOR_TYPE_ADDITIONAL_INFO) {
            buffer[i].u.additional.type = aidl_buffer[i].u.data.additional.type;
            buffer[i].u.additional.serial = aidl_buffer[i].u.data.additional.serial;
            buffer[i].u.additional.u = aidl_buffer[i].u.data.additional.u;
        } else {
            memcpy(&buffer[i].u, &aidl_buffer[i].u.data, sizeof(SensorEventPayload));
        }
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

GBinderLocalReply *HybrisBackendBinderAidl::sensorCallbackHandler(
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
    if (iface && !strcmp(iface, SENSOR_BINDER_SERVICE_IFACE_AIDL)) {
        switch (code) {
        case DYNAMIC_SENSORS_CONNECTED_AIDL:
            qCInfo(lcSensorFw) << "Dynamic sensor connected";
            break;
        case DYNAMIC_SENSORS_DISCONNECTED_AIDL:
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

void HybrisBackendBinderAidl::getSensorList()
{
    qCInfo(lcSensorFw) << "Get sensor list";
    GBinderReader reader;
    GBinderRemoteReply *reply;
    int status;

    reply = gbinder_client_transact_sync_reply(m_client, GET_SENSORS_LIST_AIDL, nullptr, &status);

    if (!reply || status != GBINDER_STATUS_OK) {
        qCWarning(lcSensorFw) << "Unable to get sensor list";
        cleanup();
        sleep(1);
        startConnect();
        return;
    }

    gbinder_remote_reply_init_reader(reply, &reader);
    gint32 count = 0;
    gbinder_reader_read_int32(&reader, &status);
    gbinder_reader_read_int32(&reader, &count);

    m_sensorCount = count;
    m_sensorArray = new sensor_t[m_sensorCount];

    for (int i = 0 ; i < m_sensorCount ; i++) {
        // Parcelable non-null
        gbinder_reader_read_int32(&reader, nullptr);
        // Parcelable size
        gbinder_reader_read_int32(&reader, nullptr);

        gbinder_reader_read_int32(&reader, &m_sensorArray[i].handle);
        m_sensorArray[i].name.data.str = gbinder_reader_read_string16(&reader);
        m_sensorArray[i].vendor.data.str = gbinder_reader_read_string16(&reader);
        gbinder_reader_read_int32(&reader, &m_sensorArray[i].version);
        gbinder_reader_read_int32(&reader, &m_sensorArray[i].type);
        m_sensorArray[i].typeAsString.data.str = gbinder_reader_read_string16(&reader);
        gbinder_reader_read_float(&reader, &m_sensorArray[i].maxRange);
        gbinder_reader_read_float(&reader, &m_sensorArray[i].resolution);
        gbinder_reader_read_float(&reader, &m_sensorArray[i].power);
        gbinder_reader_read_int32(&reader, &m_sensorArray[i].minDelay);
        gbinder_reader_read_uint32(&reader, &m_sensorArray[i].fifoReservedEventCount);
        gbinder_reader_read_uint32(&reader, &m_sensorArray[i].fifoMaxEventCount);
        m_sensorArray[i].requiredPermission.data.str = gbinder_reader_read_string16(&reader);
        gbinder_reader_read_int32(&reader, &m_sensorArray[i].maxDelay);
        gbinder_reader_read_uint32(&reader, &m_sensorArray[i].flags);
    }
    gbinder_remote_reply_unref(reply);

    m_manager->initManager();

    qCWarning(lcSensorFw) << "Hybris sensor manager initialized";
}

void HybrisBackendBinderAidl::binderDied(GBinderRemoteObject *, void *)
{
    qCWarning(lcSensorFw) << "Sensor service died! Restart sensorfw.";
    // Sensor connections don't recover correctly without restart at the moment
    QCoreApplication::exit(EXIT_FAILURE);
}

void HybrisBackendBinderAidl::startConnect()
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

void HybrisBackendBinderAidl::finishConnect()
{
    int initializeCode;
    m_remote = gbinder_servicemanager_get_service_sync(m_serviceManager,
                                    SENSOR_BINDER_SERVICE_NAME_AIDL, nullptr);

    if (m_remote) {
        qCInfo(lcSensorFw) << "Connected to sensor AIDL service";
        m_sensorInterfaceEnum = SENSOR_INTERFACE_AIDL;
        initializeCode = INITIALIZE_AIDL;
    }

    if (m_remote) {
        qCInfo(lcSensorFw) << "Initialize sensor service";
        m_deathId = gbinder_remote_object_add_death_handler(m_remote, binderDied, this);
        m_client = gbinder_client_new(m_remote, SENSOR_BINDER_SERVICE_IFACE_AIDL);
        if (!m_client) {
            qCInfo(lcSensorFw) << "Could not create client for sensor service. Trying to reconnect.";
        } else {
            GBinderRemoteReply *reply;
            GBinderLocalRequest *req = gbinder_client_new_request(m_client);
            int32_t status;
            GBinderWriter writer;

            gbinder_local_request_init_writer(req, &writer);
            m_sensorCallback = gbinder_servicemanager_new_local_object(
                m_serviceManager,
                SENSOR_BINDER_SERVICE_CALLBACK_IFACE_AIDL,
                sensorCallbackHandler,
                this);
            gbinder_local_object_set_stability(m_sensorCallback, GBINDER_STABILITY_VINTF);

            m_eventQueue = gbinder_fmq_new(sizeof(sensors_event_t_aidl), 128,
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
                int error = 0;
                GBinderReader reader;
                gbinder_remote_reply_init_reader(reply, &reader);
                gbinder_reader_read_int32(&reader, &status);
                if (status) {
                    error = status;
                }

                gbinder_remote_reply_unref(reply);
                if (!error) {
                    getSensorList();
                    return;
                } else {
                    qCWarning(lcSensorFw) << "Initialize failed with error" << error << ". Trying to reconnect.";
                }
            }
        }
    }
    // On failure cleanup and wait before reconnecting
    cleanup();
    sleep(1);
    startConnect();
}
