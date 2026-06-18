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

#ifndef HYBRISBACKEND_HAL_H
#define HYBRISBACKEND_HAL_H

#include "hybrisbackend.h"

class HybrisBackendHal : public HybrisBackend
{
public:
    HybrisBackendHal(HybrisManager *manager);
    ~HybrisBackendHal();
    static HybrisBackend *getBackend(HybrisManager *manager);
    void initialize() override;
    bool needsReaderThread() override;
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
    int setActive(int handle, bool active) override;
    int setDelay(int handle, int64_t delay_ns) override;
    void readEvents() override;

private:
    struct sensors_module_t *m_halModule;
    sensors_poll_device_1_t *m_halDevice;
    const struct sensor_t   *m_sensorArray;   // [m_sensorCount]
};

#endif // HYBRISBACKEND_HAL_H
