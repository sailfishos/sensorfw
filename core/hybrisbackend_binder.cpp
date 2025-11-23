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

#ifdef USE_BINDER

#include "hybrisbackend_binder.h"
#include "hybrisbackend_binder_aidl.h"
#include "hybrisbackend_binder_hidl.h"
#include "hybrisadaptor.h"

#include <unistd.h>

HybrisBackend *getBackend(HybrisManager *manager)
{
    HybrisBackend *backend = NULL;
    for (int i = 0; i < 5; ++i) {
        if (HybrisBackendBinderAidl::isSupported()) {
            backend = qobject_cast<HybrisBackend *>(new HybrisBackendBinderAidl(manager));
            break;
        } else if (HybrisBackendBinderHidl::isSupported()) {
            backend = qobject_cast<HybrisBackend *>(new HybrisBackendBinderHidl(manager));
            break;
        }
        sleep(1);
    }
    return backend;
}

HybrisBackendBinder::HybrisBackendBinder(HybrisManager *manager, QObject *parent)
    : HybrisBackend(manager, parent)
    , m_client(NULL)
    , m_deathId(0)
    , m_remote(NULL)
    , m_serviceManager(NULL)
    , m_sensorInterfaceEnum(SENSOR_INTERFACE_COUNT)
    , m_sensorCallback(NULL)
    , m_sensorArray(NULL)
{
}

HybrisBackendBinder::~HybrisBackendBinder()
{
}

void HybrisBackendBinder::cleanup()
{
    qCInfo(lcSensorFw) << "cleanup";
    gbinder_remote_object_remove_handler(m_remote, m_deathId);
    m_deathId = 0;

    gbinder_local_object_unref(m_sensorCallback);
    m_sensorCallback = NULL;

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
}

bool HybrisBackendBinder::needsWakeup(const sensors_event_t *eve)
{
    int index = m_manager->indexForHandle(eve->sensor);
    const struct sensor_t *sensor = &m_sensorArray[index];
    if (sensor->flags & SENSOR_FLAG_WAKE_UP) {
        return true;
    }
    return false;
}

bool HybrisBackendBinder::isValid(const sensors_event_t *eve)
{
    Q_UNUSED(eve);
    return true;
}

int HybrisBackendBinder::handle(int index)
{
    return m_sensorArray[index].handle;
}

int HybrisBackendBinder::type(int index)
{
    return m_sensorArray[index].type;
}

bool HybrisBackendBinder::isWakeupSensor(int index)
{
    return m_sensorArray[index].flags & SENSOR_FLAG_WAKE_UP;
}

void HybrisBackendBinder::initFallbackEvent(int index, sensors_event_t *eve)
{
    eve->sensor  = m_sensorArray[index].handle;
    eve->type    = m_sensorArray[index].type;
    switch (m_sensorArray[index].type) {
    case SENSOR_TYPE_LIGHT:
        // Roughly indoor lightning
        eve->u.scalar = 400;
        break;
    case SENSOR_TYPE_PROXIMITY:
        // Not-covered
        eve->u.scalar = m_sensorArray[index].maxRange;
        break;
    default:
        eve->sensor  = 0;
        eve->type    = 0;
        break;
    }
}

const char *HybrisBackendBinder::sensorName(int index)
{
    return m_sensorArray[index].name.data.str ?: "unknown";
}

int HybrisBackendBinder::maxDelay(int index)
{
    return m_sensorArray[index].maxDelay;
}

int HybrisBackendBinder::minDelay(int index)
{
    return m_sensorArray[index].minDelay;
}

float HybrisBackendBinder::maxRange(int index)
{
    return m_sensorArray[index].maxRange;
}

float HybrisBackendBinder::resolution(int index)
{
    return m_sensorArray[index].resolution;
}

#endif
