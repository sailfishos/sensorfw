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

#ifndef HybrisBackendHal_H
#define HybrisBackendHal_H

#ifndef USE_BINDER

#include <QObject>

#include "hybrisbackend.h"

class HybrisBackendHal : public HybrisBackend
{
    Q_OBJECT
public:
    HybrisBackendHal(HybrisManager *manager, QObject *parent = 0);
    ~HybrisBackendHal();
    void initialize();
    void cleanup();
    bool needsReaderThread();
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
    int setActive(int handle, bool active);
    int setDelay(int handle, int64_t delay_ns);
    void eventReaderThreadImpl();

private:
    struct sensors_module_t *m_halModule;
    sensors_poll_device_1_t *m_halDevice;
    const struct sensor_t   *m_sensorArray;   // [m_sensorCount]
};

#endif

#endif // HybrisBackendHal_H
