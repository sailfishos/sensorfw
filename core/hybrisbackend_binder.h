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

#ifndef HybrisBackendBinder_H
#define HybrisBackendBinder_H

#ifdef USE_BINDER
#include <QObject>

#include <gbinder.h>
#include "hybrisbackend.h"

class HybrisBackendBinder : public HybrisBackend
{
    Q_OBJECT
public:
    HybrisBackendBinder(HybrisManager *manager, QObject *parent = 0);
    virtual ~HybrisBackendBinder() = 0;
    virtual void initialize() = 0;
    void cleanup();
    virtual bool needsReaderThread() = 0;
    bool needsWakeup(const sensors_event_t *eve);
    bool isValid(const sensors_event_t *eve);
    void initFallbackEvent(int index, sensors_event_t *eve);
    int handle(int index);
    int type(int index);
    bool isWakeupSensor(int index);
    const char *sensorName(int index);
    int maxDelay(int index);
    int minDelay(int index);
    float maxRange(int index);
    float resolution(int index);
    virtual int setActive(int handle, bool active) = 0;
    virtual int setDelay(int handle, int64_t delay_ns) = 0;
    virtual void eventReaderThreadImpl() = 0;

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

#endif

#endif // HybrisBackendBinder_H
