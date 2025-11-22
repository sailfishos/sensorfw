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

#ifndef HybrisBackend_H
#define HybrisBackend_H

#include <QObject>

namespace {
/* Maximum number of events to transmit over event pipe in one go.
 * Also defines maximum number of events to ask from android hal/service.
 */
const size_t maxEvents = 64;
}

#ifdef USE_BINDER
#include <gbinder.h>
#include "hybrisbindertypes.h"
#else
#include <hardware/hardware.h>
#include <hardware/sensors.h>
#endif

class HybrisManager;

class HybrisBackend : public QObject
{
    Q_OBJECT
public:
    HybrisBackend(HybrisManager *manager, QObject *parent = 0);
    virtual void initialize() = 0;
    virtual void cleanup() = 0;
    int sensorCount();
    virtual bool needsReaderThread() = 0;
    virtual bool needsWakeup(const sensors_event_t *eve) = 0;
    virtual bool isValid(const sensors_event_t *eve) = 0;
    virtual void initFallbackEvent(int index, sensors_event_t *eve) = 0;
    virtual int handle(int index) = 0;
    virtual int type(int index) = 0;
    virtual bool isWakeupSensor(int index) = 0;
    virtual const char *sensorName(int index) = 0;
    virtual int maxDelay(int index) = 0;
    virtual int minDelay(int index) = 0;
    virtual float maxRange(int index) = 0;
    virtual float resolution(int index) = 0;
    virtual int setActive(int handle, bool active) = 0;
    virtual int setDelay(int handle, int64_t delay_ns) = 0;
    virtual void eventReaderThreadImpl() = 0;

protected:
    HybrisManager *m_manager;
    int m_sensorCount;
};

HybrisBackend *getBackend(HybrisManager *manager);

#endif // HybrisBackend_H
