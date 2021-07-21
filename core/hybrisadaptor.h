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

#ifndef HybrisAdaptor_H
#define HybrisAdaptor_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QFile>

#include "deviceadaptor.h"

#ifdef USE_BINDER
#include <gbinder.h>
#include "hybrisbindertypes.h"
#else
#include <hardware/sensors.h>
#include <pthread.h>
#endif

/* Older devices probably have old android hal and thus do
 * not define sensor all sensor types that have been added
 * later on -> In order to both use symbolic names and
 * compile for all devices we need to fill in holes that
 * android hal for some particular device might have.
 */
#ifndef SENSOR_TYPE_META_DATA
#define SENSOR_TYPE_META_DATA                        (0)
#endif
#ifndef SENSOR_TYPE_ACCELEROMETER
#define SENSOR_TYPE_ACCELEROMETER                    (1)
#endif
#ifndef SENSOR_TYPE_GEOMAGNETIC_FIELD
#define SENSOR_TYPE_GEOMAGNETIC_FIELD                (2) // alias for SENSOR_TYPE_MAGNETIC_FIELD
#endif
#ifndef SENSOR_TYPE_MAGNETIC_FIELD
#define SENSOR_TYPE_MAGNETIC_FIELD                   (2) // alias for SENSOR_TYPE_GEOMAGNETIC_FIELD
#endif
#ifndef SENSOR_TYPE_ORIENTATION
#define SENSOR_TYPE_ORIENTATION                      (3)
#endif
#ifndef SENSOR_TYPE_GYROSCOPE
#define SENSOR_TYPE_GYROSCOPE                        (4)
#endif
#ifndef SENSOR_TYPE_LIGHT
#define SENSOR_TYPE_LIGHT                            (5)
#endif
#ifndef SENSOR_TYPE_PRESSURE
#define SENSOR_TYPE_PRESSURE                         (6)
#endif
#ifndef SENSOR_TYPE_TEMPERATURE
#define SENSOR_TYPE_TEMPERATURE                      (7)
#endif
#ifndef SENSOR_TYPE_PROXIMITY
#define SENSOR_TYPE_PROXIMITY                        (8)
#endif
#ifndef SENSOR_TYPE_GRAVITY
#define SENSOR_TYPE_GRAVITY                          (9)
#endif
#ifndef SENSOR_TYPE_LINEAR_ACCELERATION
#define SENSOR_TYPE_LINEAR_ACCELERATION             (10)
#endif
#ifndef SENSOR_TYPE_ROTATION_VECTOR
#define SENSOR_TYPE_ROTATION_VECTOR                 (11)
#endif
#ifndef SENSOR_TYPE_RELATIVE_HUMIDITY
#define SENSOR_TYPE_RELATIVE_HUMIDITY               (12)
#endif
#ifndef SENSOR_TYPE_AMBIENT_TEMPERATURE
#define SENSOR_TYPE_AMBIENT_TEMPERATURE             (13)
#endif
#ifndef SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED
#define SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED     (14)
#endif
#ifndef SENSOR_TYPE_GAME_ROTATION_VECTOR
#define SENSOR_TYPE_GAME_ROTATION_VECTOR            (15)
#endif
#ifndef SENSOR_TYPE_GYROSCOPE_UNCALIBRATED
#define SENSOR_TYPE_GYROSCOPE_UNCALIBRATED          (16)
#endif
#ifndef SENSOR_TYPE_SIGNIFICANT_MOTION
#define SENSOR_TYPE_SIGNIFICANT_MOTION              (17)
#endif
#ifndef SENSOR_TYPE_STEP_DETECTOR
#define SENSOR_TYPE_STEP_DETECTOR                   (18)
#endif
#ifndef SENSOR_TYPE_STEP_COUNTER
#define SENSOR_TYPE_STEP_COUNTER                    (19)
#endif
#ifndef SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR
#define SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR     (20)
#endif
#ifndef SENSOR_TYPE_HEART_RATE
#define SENSOR_TYPE_HEART_RATE                      (21)
#endif
#ifndef SENSOR_TYPE_TILT_DETECTOR
#define SENSOR_TYPE_TILT_DETECTOR                   (22)
#endif
#ifndef SENSOR_TYPE_WAKE_GESTURE
#define SENSOR_TYPE_WAKE_GESTURE                    (23)
#endif
#ifndef SENSOR_TYPE_GLANCE_GESTURE
#define SENSOR_TYPE_GLANCE_GESTURE                  (24)
#endif
#ifndef SENSOR_TYPE_PICK_UP_GESTURE
#define SENSOR_TYPE_PICK_UP_GESTURE                 (25)
#endif
#ifndef SENSOR_TYPE_WRIST_TILT_GESTURE
#define SENSOR_TYPE_WRIST_TILT_GESTURE              (26)
#endif

#define GRAVITY_RECIPROCAL_THOUSANDS 101.971621298
#define RADIANS_TO_DEGREESECONDS 57295.7795
#define RADIANS_TO_DEGREES 57.2957795

#define SENSORFW_MCE_WATCHER

class HybrisAdaptor;

struct HybrisSensorState
{
    HybrisSensorState();
    ~HybrisSensorState();

    int  m_minDelay;
    int  m_maxDelay;
    int  m_delay;
    int  m_active;
    sensors_event_t m_fallbackEvent;
};

class HybrisManager : public QObject
{
    Q_OBJECT
public:
    static HybrisManager *instance();

    explicit HybrisManager(QObject *parent = 0);
    virtual ~HybrisManager();
    void cleanup();
    void initManager();

    /* - - - - - - - - - - - - - - - - - - - *
     * android sensor functions
     * - - - - - - - - - - - - - - - - - - - */

    sensors_event_t *eventForHandle(int handle) const;
    int              indexForHandle(int handle) const;
    int              indexForType  (int sensorType) const;
    int              handleForType (int sensorType) const;
    float            getMaxRange   (int handle) const;
    float            getResolution (int handle) const;
    int              getMinDelay   (int handle) const;
    int              getMaxDelay   (int handle) const;
    int              getDelay      (int handle) const;
    bool             setDelay      (int handle, int delay_ms, bool force);
    bool             getActive     (int handle) const;
    bool             setActive     (int handle, bool active);

    /* - - - - - - - - - - - - - - - - - - - *
     * HybrisManager <--> sensorfwd
     * - - - - - - - - - - - - - - - - - - - */

    void startReader     (HybrisAdaptor *adaptor);
    void stopReader      (HybrisAdaptor *adaptor);
    void registerAdaptor (HybrisAdaptor * adaptor);
    void processSample   (const sensors_event_t& data);

private:
    // fields
    bool                          m_initialized;
    QMap <int, HybrisAdaptor *>   m_registeredAdaptors; // type -> obj

#ifdef USE_BINDER
    // Binder backend
    GBinderClient                *m_client;
    gulong                        m_deathId;
    gulong                        m_pollTransactId;
    GBinderRemoteObject          *m_remote;
    GBinderServiceManager        *m_serviceManager;
    struct sensor_t              *m_sensorArray;   // [m_sensorCount]
#else
    // HAL backend
    struct sensors_module_t      *m_halModule;
    struct sensors_poll_device_t *m_halDevice;
    pthread_t                     m_halEventReaderTid;
    const struct sensor_t        *m_sensorArray;   // [m_sensorCount]
#endif
    int                           m_sensorCount;
    HybrisSensorState            *m_sensorState;   // [m_sensorCount]
    QMap <int, int>               m_indexOfType;   // type   -> index
    QMap <int, int>               m_indexOfHandle; // handle -> index

#ifdef USE_BINDER
    void getSensorList();
    void startConnect();
    void finishConnect();
    static void binderDied(GBinderRemoteObject *, void *user_data);
    void pollEvents();
    static void pollEventsCallback(
        GBinderClient* /*client*/, GBinderRemoteReply* reply,
        int status, void* userData);
#endif

    friend class HybrisAdaptorReader;

#ifndef USE_BINDER
private:
    static void *halEventReaderThread(void *aptr);
#endif
    float scaleSensorValue(const float value, const int type) const;
    void processEvents(const sensors_event_t *buffer,
        int numberOfEvents, bool &blockSuspend, bool &errorInInput);
};

class HybrisAdaptor : public DeviceAdaptor
{
public:
    HybrisAdaptor(const QString& id, int type);
    virtual ~HybrisAdaptor();

    virtual void init();

    virtual bool startAdaptor();
    bool         isRunning() const;
    virtual void stopAdaptor();

    void         evaluateSensor();
    virtual bool startSensor();
    virtual void stopSensor();

    virtual bool standby();
    virtual bool resume();

    virtual void sendInitialData();

    friend class HybrisManager;

protected:
    virtual void processSample(const sensors_event_t& data) = 0;

    qreal        minRange() const;
    qreal        maxRange() const;
    qreal        resolution() const;

    unsigned int minInterval() const;
    unsigned int maxInterval() const;

    virtual unsigned int interval() const;
    virtual bool setInterval(const unsigned int value, const int sessionId);
    virtual unsigned int evaluateIntervalRequests(int& sessionId) const;
    static bool writeToFile(const QByteArray& path, const QByteArray& content);

private:
    bool          m_inStandbyMode;
    volatile bool m_isRunning;
    bool          m_shouldBeRunning;

    int           m_sensorHandle;
    int           m_sensorType;
};

#endif // HybrisAdaptor_H
