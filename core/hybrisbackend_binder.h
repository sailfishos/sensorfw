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

#ifndef HYBRISBACKEND_BINDER_H
#define HYBRISBACKEND_BINDER_H

#include <gbinder.h>
#include "hybrisbackend.h"

class HybrisBackendBinder : public HybrisBackend
{
public:
    HybrisBackendBinder(HybrisManager *manager);
    virtual ~HybrisBackendBinder() {}
    static HybrisBackend *getBackend(HybrisManager *manager);
    void cleanup();
    bool needsWakeup(const sensors_event_t *eve) override;
    bool isValid(const sensors_event_t *eve) override;
    void initFallbackEvent(int index, sensors_event_t *eve) override;
    int handle(int index) override;
    int type(int index) override;
    bool isWakeupSensor(int index) override;
    const char *sensorName(int index) override;
    int maxDelay(int index) override;
    int minDelay(int index) override;
    float maxRange(int index) override;
    float resolution(int index) override;

protected:
    GBinderClient         *m_client;
    gulong                 m_deathId;
    GBinderRemoteObject   *m_remote;
    GBinderServiceManager *m_serviceManager;
    SENSOR_INTERFACE       m_sensorInterfaceEnum;
    GBinderLocalObject    *m_sensorCallback;
    GBinderFmq            *m_eventQueue;
    GBinderFmq            *m_wakeLockQueue;
    struct sensor_t       *m_sensorArray;   // [m_sensorCount]
};

#endif // HYBRISBACKEND_BINDER_H
