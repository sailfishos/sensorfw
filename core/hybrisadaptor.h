/****************************************************************************
**
** Copyright (c) 2013 Jolla Ltd.
** Copyright (c) 2025 Jollyboys Ltd.
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
#include <QSocketNotifier>

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
#ifndef SENSOR_TYPE_MAGNETIC_FIELD
#define SENSOR_TYPE_MAGNETIC_FIELD                   (2)
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
#ifndef SENSOR_TYPE_DEVICE_ORIENTATION
#define SENSOR_TYPE_DEVICE_ORIENTATION              (27)
#endif
#ifndef SENSOR_TYPE_POSE_6DOF
#define SENSOR_TYPE_POSE_6DOF                       (28)
#endif
#ifndef SENSOR_TYPE_STATIONARY_DETECT
#define SENSOR_TYPE_STATIONARY_DETECT               (29)
#endif
#ifndef SENSOR_TYPE_MOTION_DETECT
#define SENSOR_TYPE_MOTION_DETECT                   (30)
#endif
#ifndef SENSOR_TYPE_HEART_BEAT
#define SENSOR_TYPE_HEART_BEAT                      (31)
#endif
#ifndef SENSOR_TYPE_DYNAMIC_SENSOR_META
#define SENSOR_TYPE_DYNAMIC_SENSOR_META             (32)
#endif
#ifndef SENSOR_TYPE_ADDITIONAL_INFO
#define SENSOR_TYPE_ADDITIONAL_INFO                 (33)
#endif
#ifndef SENSOR_TYPE_LOW_LATENCY_OFFBODY_DETECT
#define SENSOR_TYPE_LOW_LATENCY_OFFBODY_DETECT      (34)
#endif
#ifndef SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED
#define SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED      (35)
#endif
#ifndef SENSOR_TYPE_HINGE_ANGLE
#define SENSOR_TYPE_HINGE_ANGLE                     (36)
#endif
#ifndef SENSOR_TYPE_HEAD_TRACKER
#define SENSOR_TYPE_HEAD_TRACKER                    (37)
#endif
#ifndef SENSOR_TYPE_ACCELEROMETER_LIMITED_AXES
#define SENSOR_TYPE_ACCELEROMETER_LIMITED_AXES      (38)
#endif
#ifndef SENSOR_TYPE_GYROSCOPE_LIMITED_AXES
#define SENSOR_TYPE_GYROSCOPE_LIMITED_AXES          (39)
#endif
#ifndef SENSOR_TYPE_ACCELEROMETER_LIMITED_AXES_UNCALIBRATED
#define SENSOR_TYPE_ACCELEROMETER_LIMITED_AXES_UNCALIBRATED (40)
#endif
#ifndef SENSOR_TYPE_GYROSCOPE_LIMITED_AXES_UNCALIBRATED
#define SENSOR_TYPE_GYROSCOPE_LIMITED_AXES_UNCALIBRATED (41)
#endif
#ifndef SENSOR_TYPE_HEADING
#define SENSOR_TYPE_HEADING                         (42)
#endif
#ifndef SENSOR_TYPE_DEVICE_PRIVATE_BASE
#define SENSOR_TYPE_DEVICE_PRIVATE_BASE             (65536)
#endif

/* Legacy aliases */
#ifndef SENSOR_TYPE_GEOMAGNETIC_FIELD
#define SENSOR_TYPE_GEOMAGNETIC_FIELD               SENSOR_TYPE_MAGNETIC_FIELD
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

    int  m_minDelay_us;
    int  m_maxDelay_us;
    int  m_delay_us;
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
    bool             setDelay      (int handle, int delay_us, bool force);
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
    QMultiMap <int, HybrisAdaptor *>   m_registeredAdaptors; // type -> obj

#ifdef USE_BINDER
    // Binder backend
    GBinderClient                *m_client;
    gulong                        m_deathId;
    gulong                        m_pollTransactId;
    GBinderRemoteObject          *m_remote;
    GBinderServiceManager        *m_serviceManager;
    SENSOR_INTERFACE              m_sensorInterfaceEnum;
    GBinderLocalObject           *m_sensorCallback;
    GBinderFmq                   *m_eventQueue;
    GBinderFmq                   *m_wakeLockQueue;
    struct sensor_t              *m_sensorArray;   // [m_sensorCount]
#else
    // HAL backend
    struct sensors_module_t      *m_halModule;
    sensors_poll_device_1_t      *m_halDevice;
    const struct sensor_t        *m_sensorArray;   // [m_sensorCount]
#endif
    pthread_t                     m_eventReaderTid;
    int                           m_sensorCount;
    HybrisSensorState            *m_sensorState;   // [m_sensorCount]
    QMap <int, int>               m_indexOfType;   // type   -> index
    QMap <int, int>               m_indexOfHandle; // handle -> index
    int                           m_eventPipeReadFd;
    int                           m_eventPipeWriteFd;
    QSocketNotifier              *m_eventPipeNotifier;
    QSet<int>                     m_doubleStopReaderQuirkSensorTypes;

#ifdef USE_BINDER
    static GBinderLocalReply *sensorCallbackHandler(
        GBinderLocalObject* obj,
        GBinderRemoteRequest* req,
        guint code,
        guint flags,
        int* status,
        void* user_data);
    void getSensorList();
    void startConnect();
    void finishConnect();
    static void binderDied(GBinderRemoteObject *, void *user_data);
    void pollEvents();
    static void pollEventsCallback(
        GBinderClient* /*client*/, GBinderRemoteReply* reply,
        int status, void* userData);
    bool typeRequiresWakeup(int type);
#endif

    friend class HybrisAdaptorReader;

private:
    static void *eventReaderThread(void *aptr);
    float scaleSensorValue(const float value, const int type) const;
    void initEventPipe();
    void cleanupEventPipe();
    void eventPipeWakeup(int fd);
    int queueEvents(const sensors_event_t *buffer, int numEvents);
    int processEvents(const sensors_event_t *buffer, int numEvents);
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
    virtual bool setInterval(const int sessionId, const unsigned int interval_us);
    static bool writeToFile(const QByteArray& path, const QByteArray& content);

private:
    bool          m_inStandbyMode;
    volatile bool m_isRunning;
    bool          m_shouldBeRunning;

    int           m_sensorHandle;
    int           m_sensorType;
};

#endif // HybrisAdaptor_H
