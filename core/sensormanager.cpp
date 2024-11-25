/**
   @file sensormanager.cpp
   @brief SensorManager

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Semi Malinen <semi.malinen@nokia.com
   @author Joep van Gassel <joep.van.gassel@nokia.com>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Lihan Guo <ext-lihan.4.guo@nokia.com>

   This file is part of Sensord.

   Sensord is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License
   version 2.1 as published by the Free Software Foundation.

   Sensord is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Sensord.  If not, see <http://www.gnu.org/licenses/>.
   </p>
 */

#include "sensormanager_a.h"
#include "serviceinfo.h"
#include "sensormanager.h"
#include "loader.h"
#include "idutils.h"
#include "logging.h"
#ifdef SENSORFW_MCE_WATCHER
#include "mcewatcher.h"
#endif // SENSORFW_MCE_WATCHER
#ifdef SENSORFW_LUNA_SERVICE_CLIENT
#include "lsclient.h"
#endif // SENSORFW_LUNA_SERVICE_CLIENT
#include <QSocketNotifier>
#include <errno.h>
#include <functional>
#include "sockethandler.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <QTimer>
#include <QSettings>


typedef struct {
    int id;
    int size;
    void* buffer;
} PipeData;

const int SensorManager::SOCKET_CONNECTION_TIMEOUT_MS = 10000;

SensorManager* SensorManager::instance_ = NULL;
int SensorManager::sessionIdCount_ = 0;

SensorInstanceEntry::SensorInstanceEntry(const QString& type) :
    sensor_(0),
    type_(type)
{
}

SensorInstanceEntry::~SensorInstanceEntry()
{
}

ChainInstanceEntry::ChainInstanceEntry(const QString& type) :
    cnt_(0),
    chain_(0),
    type_(type)
{
}

ChainInstanceEntry::~ChainInstanceEntry()
{
}

DeviceAdaptorInstanceEntry::DeviceAdaptorInstanceEntry(const QString& type, const QString& id) :
    adaptor_(0),
    cnt_(0),
    type_(type)
{
    propertyMap_ = ParameterParser::getPropertyMap(id);
}

DeviceAdaptorInstanceEntry::~DeviceAdaptorInstanceEntry()
{
}

SessionInstanceEntry::SessionInstanceEntry(QObject* parent, int sessionId, const QString& clientName)
    : QObject(parent)
    , m_sessionId(sessionId)
    , m_clientName(clientName)
    , m_timer(nullptr)
{
}

SessionInstanceEntry::~SessionInstanceEntry()
{
    delete m_timer;
    m_timer = nullptr;
}

void SessionInstanceEntry::expectConnection(int msec)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(msec);
    connect(m_timer, &QTimer::timeout, this, &SessionInstanceEntry::timerTimeout);
    m_timer->start();

    SocketHandler& socketHandler = SensorManager::instance().socketHandler();
    connect(&socketHandler, &SocketHandler::connectedSession,
            this, &SessionInstanceEntry::sessionConnected);
}

void SessionInstanceEntry::timerTimeout()
{
    m_timer->deleteLater();
    m_timer = nullptr;

    SocketHandler& socketHandler = SensorManager::instance().socketHandler();
    disconnect(&socketHandler, &SocketHandler::connectedSession, this, nullptr);
    socketHandler.checkConnectionEstablished(m_sessionId);
}

void SessionInstanceEntry::sessionConnected(int sessionId)
{
    if (sessionId == m_sessionId) {
        if (m_timer) {
            m_timer->stop();
            m_timer->deleteLater();
            m_timer = nullptr;
        }

        SocketHandler& socketHandler = SensorManager::instance().socketHandler();
        disconnect(&socketHandler, &SocketHandler::connectedSession, this, nullptr);
    }
}

inline QDBusConnection bus()
{
    return QDBusConnection::systemBus();
}

SensorManager& SensorManager::instance()
{
    if (!instance_) {
        instance_ = new SensorManager;
    }

    return *instance_;
}

SensorManager::SensorManager()
    : errorCode_(SmNoError),
    pipeNotifier_(0),
    deviation(0)
{
    QString pluginPath;
    const char* SOCKET_NAME = "/run/sensord.sock";
    QByteArray env = qgetenv("SENSORFW_SOCKET_PATH");
    if (!env.isEmpty()) {
      env += SOCKET_NAME;
      SOCKET_NAME = env;
    }

    new SensorManagerAdaptor(this);

    socketHandler_ = new SocketHandler(this);
    connect(socketHandler_, SIGNAL(lostSession(int)), this, SLOT(lostClient(int)));

    Q_ASSERT(socketHandler_->listen(SOCKET_NAME));

    if (pipe(pipefds_) == -1) {
        qCCritical(lcSensorFw) << "Failed to create pipe: " << strerror(errno);
        pipefds_[0] = pipefds_[1] = 0;
    } else {
        pipeNotifier_ = new QSocketNotifier(pipefds_[0], QSocketNotifier::Read);
        connect(pipeNotifier_, SIGNAL(activated(int)), this, SLOT(sensorDataHandler(int)));
    }

    if (chmod(SOCKET_NAME, S_IRWXU|S_IRWXG|S_IRWXO) != 0) {
        qCWarning(lcSensorFw) << "Error setting socket permissions! " << SOCKET_NAME;
    }

    serviceWatcher_ = new QDBusServiceWatcher(this);
    serviceWatcher_->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    QObject::connect(serviceWatcher_, &QDBusServiceWatcher::serviceUnregistered,
                     this, &SensorManager::dbusClientUnregistered);

#ifdef SENSORFW_MCE_WATCHER
    mceWatcher_ = new MceWatcher(this);
    connect(mceWatcher_, SIGNAL(displayStateChanged(const bool)),
            this, SLOT(displayStateChanged(const bool)));

    connect(mceWatcher_, SIGNAL(devicePSMStateChanged(const bool)),
            this, SLOT(devicePSMStateChanged(const bool)));

#endif //SENSORFW_MCE_WATCHER

#ifdef SENSORFW_LUNA_SERVICE_CLIENT

    lsClient_ = new LSClient(this);
    connect(lsClient_, SIGNAL(displayStateChanged(const bool)),
            this, SLOT(displayStateChanged(const bool)));

    connect(lsClient_, SIGNAL(devicePSMStateChanged(const bool)),
            this, SLOT(devicePSMStateChanged(const bool)));

#endif //SENSORFW_LUNA_SERVICE_CLIENT
}

SensorManager::~SensorManager()
{
    // stop adaptor threads and acquired resources
    for (QMap<QString, DeviceAdaptorInstanceEntry>::const_iterator it = deviceAdaptorInstanceMap_.begin();
         it != deviceAdaptorInstanceMap_.end(); ++it) {
        releaseDeviceAdaptor(it.key());
    }

    sleep(1); // sleep for seconds so adaptor threads have time to die

    // close open sessions
    for (QMap<QString, SensorInstanceEntry>::const_iterator it = sensorInstanceMap_.begin();
        it != sensorInstanceMap_.end(); ++it) {
        for (QSet<int>::const_iterator it2 = it.value().sessions_.begin(); it2 != it.value().sessions_.end(); ++it2) {
            lostClient(*it2);
        }
    }

    // delete sensors
    for (QMap<QString, SensorInstanceEntry>::iterator it = sensorInstanceMap_.begin();
         it != sensorInstanceMap_.end(); ++it) {
        if (it.value().sensor_) {
            delete it.value().sensor_;
            it.value().sensor_ = 0;
        }
    }

    // delete chains
    for (QMap<QString, ChainInstanceEntry>::iterator it = chainInstanceMap_.begin();
            it != chainInstanceMap_.end(); ++it) {
        if (it.value().chain_) {
            delete it.value().chain_;
            it.value().chain_ = 0;
        }
    }

    // delete adaptors
    for (QMap<QString, DeviceAdaptorInstanceEntry>::iterator it = deviceAdaptorInstanceMap_.begin();
         it != deviceAdaptorInstanceMap_.end(); ++it) {
        if (it.value().adaptor_) {
            delete it.value().adaptor_;
            it.value().adaptor_ = 0;
        }
    }

    delete socketHandler_;
    delete pipeNotifier_;
    delete serviceWatcher_;
    if (pipefds_[0]) close(pipefds_[0]);
    if (pipefds_[1]) close(pipefds_[1]);

#ifdef SENSORFW_MCE_WATCHER
    delete mceWatcher_;
#endif //SENSORFW_MCE_WATCHER

#ifdef SENSORFW_LUNA_SERVICE_CLIENT
    delete lsClient_;
#endif //SENSORFW_LUNA_SERVICE_CLIENT
}

void SensorManager::setError(SensorManagerError errorCode, const QString& errorString)
{
    qCWarning(lcSensorFw) << "SensorManagerError: " << errorString;

    errorCode_   = errorCode;
    errorString_ = errorString;

    emit errorSignal(errorCode);
}

bool SensorManager::registerService()
{
    clearError();

    bool ok = bus().isConnected();
    if (!ok) {
        QDBusError error = bus().lastError();
        setError(SmNotConnected, error.message());
        return false;
    }

    ok = bus().registerObject( OBJECT_PATH, this );
    if (!ok) {
        QDBusError error = bus().lastError();
        setError(SmCanNotRegisterObject, error.message());
        return false;
    }

    ok = bus().registerService ( SERVICE_NAME );
    if (!ok) {
        QDBusError error = bus().lastError();
        setError(SmCanNotRegisterService, error.message());
        return false;
    }
    serviceWatcher_->setConnection(bus());
    return true;
}

AbstractSensorChannel* SensorManager::addSensor(const QString& id)
{
    qCInfo(lcSensorFw) << "Adding sensor: " << id;

    clearError();

    QString cleanId = getCleanId(id);
    QMap<QString, SensorInstanceEntry>::const_iterator entryIt = sensorInstanceMap_.find(cleanId);

    if (entryIt == sensorInstanceMap_.end()) {
        qCCritical(lcSensorFw) << QString("%1 not present").arg(cleanId);
        setError( SmIdNotRegistered, QString(tr("instance for sensor type '%1' not registered").arg(cleanId)) );
        return NULL;
    }

    const QString& typeName = entryIt.value().type_;

    if (!sensorFactoryMap_.contains(typeName)) {
        setError( SmFactoryNotRegistered, QString(tr("factory for sensor type '%1' not registered").arg(typeName)) );
        return NULL;
    }

    AbstractSensorChannel* sensorChannel = sensorFactoryMap_[typeName](id);
    if (!sensorChannel->isValid()) {
        qCCritical(lcSensorFw) << QString("%1 instantiation failed").arg(cleanId);
        delete sensorChannel;
        removeSensor(getCleanId(id));
        sensorFactoryMap_.remove(id);
        return NULL;
    }

    bool ok = bus().registerObject(OBJECT_PATH + "/" + sensorChannel->id(), sensorChannel);
    if (!ok) {
        QDBusError error = bus().lastError();
        setError(SmCanNotRegisterObject, error.message());
        qCCritical(lcSensorFw) << "Failed to register sensor '" << OBJECT_PATH + "/" + sensorChannel->id() << "'";
        delete sensorChannel;
        return nullptr;
    }
    return sensorChannel;
}

void SensorManager::removeSensor(const QString& id)
{
    qCInfo(lcSensorFw) << "SensorManager removing sensor:" << id;

    QMap<QString, SensorInstanceEntry>::iterator entryIt = sensorInstanceMap_.find(id);
    bus().unregisterObject(OBJECT_PATH + "/" + id);
    delete entryIt.value().sensor_;
    entryIt.value().sensor_ = 0;
    sensorInstanceMap_.remove(id);
}

bool SensorManager::loadPlugin(const QString& name)
{
    qCInfo(lcSensorFw) << "SensorManager loading plugin:" << name;

    QString errorMessage;
    bool result;

    Loader& l = Loader::instance();
    if (!(result = l.loadPlugin(name, &errorMessage))) {
        setError (SmCanNotRegisterObject, errorMessage);
    }
    return result;
}

QStringList SensorManager::availablePlugins() const
{
    Loader& l = Loader::instance();
    return l.availablePlugins();
}

bool SensorManager::pluginAvailable(const QString &name) const
{
    Loader& l = Loader::instance();
    return l.pluginAvailable(name);
}

QStringList SensorManager::availableSensorPlugins() const
{
    Loader& l = Loader::instance();
    return l.availableSensorPlugins();
}

int SensorManager::requestSensor(const QString& id)
{
    qCInfo(lcSensorFw) << "Requesting sensor:" << id;

    clearError();

    QString cleanId = getCleanId(id);

    qDebug() << sensorInstanceMap_.keys();

    QMap<QString, SensorInstanceEntry>::iterator entryIt = sensorInstanceMap_.find(cleanId);
    if (entryIt == sensorInstanceMap_.end()) {
        setError(SmIdNotRegistered, QString(tr("requested sensor id '%1' not registered")).arg(cleanId));
        return INVALID_SESSION;
    }

    QString clientName;
    if (calledFromDBus())
        clientName = message().service();

    int sessionId = createNewSessionId();
    if (!entryIt.value().sensor_) {
        AbstractSensorChannel* sensor = addSensor(id);
        if (sensor == nullptr) {
            setError(SmNotInstantiated, tr("sensor has not been instantiated"));
            return INVALID_SESSION;
        }
        entryIt.value().sensor_ = sensor;
    }

    entryIt.value().sessions_.insert(sessionId);
    if (!clientName.isEmpty()) {
        QMap<int, SessionInstanceEntry*>::iterator sessionIt = sessionInstanceMap_.insert(
            sessionId, new SessionInstanceEntry(this, sessionId, clientName));
        serviceWatcher_->addWatchedService(clientName);
        sessionIt.value()->expectConnection(SOCKET_CONNECTION_TIMEOUT_MS);
    }

    return sessionId;
}

bool SensorManager::releaseSensor(const QString& id, int sessionId)
{
    QString clientName;
    QMap<int, SessionInstanceEntry*>::iterator sessionIt = sessionInstanceMap_.find(sessionId);
    if (calledFromDBus()) {
        clientName = message().service();
        if (sessionIt == sessionInstanceMap_.end() || sessionIt.value()->m_clientName != clientName) {
            qCWarning(lcSensorFw) << "Ignoring attempt to release session" << sessionId
                          << "that wasn't previously registered for D-Bus client" << clientName;
            return false;
        }
    }

    qCInfo(lcSensorFw) << "Releasing sensor '" << id << "' for session: " << sessionId;

    clearError();

    // no parameter passing in release
    if (id.contains(';')) {
        qCWarning(lcSensorFw) << "Invalid parameter passed to releaseSensor(): " << id;
        return false;
    }

    QMap<QString, SensorInstanceEntry>::iterator entryIt = sensorInstanceMap_.find(id);

    if (entryIt == sensorInstanceMap_.end()) {
        setError(SmIdNotRegistered, QString(tr("requested sensor id '%1' not registered").arg(id)));
        return false;
    }

    /// Remove any property requests by this session
    entryIt.value().sensor_->removeSession(sessionId);

    if (entryIt.value().sessions_.empty()) {
        setError(SmNotInstantiated, tr("sensor has not been instantiated, no session to release"));
        return false;
    }

    bool returnValue = false;

    if (entryIt.value().sessions_.remove(sessionId)) {
        /** Fix for NB#242237
        if ( entryIt.value().sessions_.empty() )
        {
            removeSensor(id);
        }
        */
        returnValue = true;
    } else {
        // sessionId does not correspond to a request
        setError(SmNotInstantiated, tr("invalid sessionId, no session to release"));
    }

    if (sessionIt != sessionInstanceMap_.end()) {
        delete sessionIt.value();
        sessionInstanceMap_.erase(sessionIt);
    }

    if (!clientName.isEmpty()) {
        bool hasMoreSessions = false;
        for (sessionIt = sessionInstanceMap_.begin(); sessionIt != sessionInstanceMap_.end(); ++sessionIt) {
            if (sessionIt.value()->m_clientName == clientName) {
                hasMoreSessions = true;
                break;
            }
        }

        if (!hasMoreSessions)
            serviceWatcher_->removeWatchedService(clientName);
    }

    socketHandler_->removeSession(sessionId);

    return returnValue;
}

AbstractChain* SensorManager::requestChain(const QString& id)
{
    qCInfo(lcSensorFw) << "Requesting chain: " << id;
    clearError();

    AbstractChain* chain = NULL;
    QMap<QString, ChainInstanceEntry>::iterator entryIt = chainInstanceMap_.find(id);

    if (entryIt != chainInstanceMap_.end()) {
        if (entryIt.value().chain_ ) {
            chain = entryIt.value().chain_;
            entryIt.value().cnt_++;
            qCInfo(lcSensorFw) << "Found chain '" << id << "'. Ref count: " << entryIt.value().cnt_;
        } else {
            QString type = entryIt.value().type_;
            if (chainFactoryMap_.contains(type)) {
                chain = chainFactoryMap_[type](id);
                Q_ASSERT(chain);
                qCInfo(lcSensorFw) << "Instantiated chain '" << id << "'. Valid =" << chain->isValid();

                entryIt.value().cnt_++;
                entryIt.value().chain_ = chain;
            } else {
                setError( SmFactoryNotRegistered, QString(tr("unknown chain type '%1'").arg(type)) );
            }
        }
    } else {
        setError( SmIdNotRegistered, QString(tr("unknown chain id '%1'").arg(id)) );
    }

    return chain;
}

void SensorManager::releaseChain(const QString& id)
{
    qCInfo(lcSensorFw) << "Releasing chain: " << id;

    clearError();

    QMap<QString, ChainInstanceEntry>::iterator entryIt = chainInstanceMap_.find(id);
    if (entryIt != chainInstanceMap_.end()) {
        if (entryIt.value().chain_) {
            entryIt.value().cnt_--;

            /** Fix for NB#242237
            if (entryIt.value().cnt_ == 0)
            */
            if (false) {
                qCInfo(lcSensorFw) << "Chain '" << id << "' has no more references. Deleting it.";
                delete entryIt.value().chain_;
                entryIt.value().chain_ = 0;
            } else {
                qCInfo(lcSensorFw) << "Chain '" << id << "' ref count: " << entryIt.value().cnt_;
            }
        } else {
            setError( SmNotInstantiated, QString(tr("chain '%1' not instantiated, cannot release").arg(id)) );
        }
    } else {
        setError( SmIdNotRegistered, QString(tr("unknown chain id '%1'").arg(id)) );
    }
}

DeviceAdaptor* SensorManager::requestDeviceAdaptor(const QString& id)
{
    qCInfo(lcSensorFw) << "Requesting adaptor:" << id;

    clearError();
    // no parameter passing in release
    if (id.contains(';')) {
        setError( SmIdNotRegistered, QString(tr("unknown adaptor id '%1'").arg(id)) );
        return Q_NULLPTR;
    }

    DeviceAdaptor* da = NULL;
    QMap<QString, DeviceAdaptorInstanceEntry>::iterator entryIt = deviceAdaptorInstanceMap_.find(id);
    if (entryIt != deviceAdaptorInstanceMap_.end()) {
        if (entryIt.value().adaptor_) {
            Q_ASSERT( entryIt.value().adaptor_ );
            da = entryIt.value().adaptor_;
            entryIt.value().cnt_++;
            qCInfo(lcSensorFw) << "Found adaptor '" << id << "'. Ref count:" << entryIt.value().cnt_;
        } else {
            QString type = entryIt.value().type_;
            if (deviceAdaptorFactoryMap_.contains(type)) {
                da = deviceAdaptorFactoryMap_[type](id);
                Q_ASSERT( da );
                bool ok = da->isValid();
                if (ok) {
                    da->init();

                    ParameterParser::applyPropertyMap(da, entryIt.value().propertyMap_);

                    ok = da->startAdaptor();
                }
                if (ok) {
                    entryIt.value().adaptor_ = da;
                    entryIt.value().cnt_++;
                    qCInfo(lcSensorFw) << "Instantiated adaptor '" << id << "'. Valid =" << da->isValid();
                } else {
                    setError(SmAdaptorNotStarted, QString(tr("adaptor '%1' can not be started").arg(id)) );
                    delete da;
                    da = NULL;
                }
            } else {
                setError( SmFactoryNotRegistered, QString(tr("unknown adaptor type '%1'").arg(type)) );
            }
        }
    } else {
        setError( SmIdNotRegistered, QString(tr("unknown adaptor id '%1'").arg(id)) );
    }

    return da;
}

void SensorManager::releaseDeviceAdaptor(const QString& id)
{
    qCInfo(lcSensorFw) << "Releasing adaptor:" << id;

    clearError();
    // no parameter passing in release
    if (id.contains(';')) {
        setError( SmIdNotRegistered, QString(tr("unknown adaptor id '%1'").arg(id)) );
        return;
    }

    QMap<QString, DeviceAdaptorInstanceEntry>::iterator entryIt = deviceAdaptorInstanceMap_.find(id);
    if (entryIt != deviceAdaptorInstanceMap_.end()) {
        if (entryIt.value().adaptor_) {
            Q_ASSERT( entryIt.value().adaptor_ );

            entryIt.value().cnt_--;
            if (entryIt.value().cnt_ == 0) {
                qCInfo(lcSensorFw) << "Adaptor '" << id << "' has no more references.";

                Q_ASSERT( entryIt.value().adaptor_ );

                entryIt.value().adaptor_->stopAdaptor();
                /** Fix for NB#242237
                delete entryIt.value().adaptor_;
                entryIt.value().adaptor_ = 0;
                */
            } else {
                qCInfo(lcSensorFw) << "Adaptor '" << id << "' has ref count:" << entryIt.value().cnt_;
            }
        } else {
            setError(SmNotInstantiated, QString(tr("adaptor '%1' not instantiated, cannot release").arg(id)));
        }
    } else {
        setError(SmIdNotRegistered, QString(tr("unknown adaptor id '%1'").arg(id)));
    }
}

FilterBase* SensorManager::instantiateFilter(const QString& id)
{
    qCInfo(lcSensorFw) << "Instantiating filter:" << id;

    QMap<QString, FilterFactoryMethod>::iterator it = filterFactoryMap_.find(id);
    if (it == filterFactoryMap_.end()) {
        qCWarning(lcSensorFw) << "Filter " << id << " not found.";
        return nullptr;
    }
    return it.value()();
}

bool SensorManager::write(int id, const void* source, int size)
{
    void* buffer = malloc(size);
    if (!buffer) {
        qCCritical(lcSensorFw) << "Malloc failed!";
        return false;
    }
    PipeData pipeData;
    pipeData.id = id;
    pipeData.size = size;
    pipeData.buffer = buffer;

    memcpy(buffer, source, size);

    if (::write(pipefds_[1], &pipeData, sizeof(pipeData)) < (int)sizeof(pipeData)) {
        qCWarning(lcSensorFw) << "Failed to write all data to pipe.";
        return false;
    }
    return true;
}

void SensorManager::sensorDataHandler(int)
{
    PipeData pipeData;
    ssize_t bytesRead = read(pipefds_[0], &pipeData, sizeof(pipeData));

    if (!bytesRead || !socketHandler_->write(pipeData.id, pipeData.buffer, pipeData.size)) {
        qCWarning(lcSensorFw) << "Failed to write data to socket.";
    }

    free(pipeData.buffer);
}

void SensorManager::lostClient(int sessionId)
{
    for (QMap<QString, SensorInstanceEntry>::iterator it = sensorInstanceMap_.begin();
         it != sensorInstanceMap_.end(); ++it) {
        if (it.value().sessions_.contains(sessionId)) {
            qCInfo(lcSensorFw) << "[SensorManager]: Lost session " << sessionId << " detected as " << it.key();

            qCInfo(lcSensorFw) << "[SensorManager]: Stopping sessionId " << sessionId;
            it.value().sensor_->stop(sessionId);

            qCInfo(lcSensorFw) << "[SensorManager]: Releasing sessionId " << sessionId;
            releaseSensor(it.key(), sessionId);
            return;
        }
    }
    qCWarning(lcSensorFw) << "[SensorManager]: Lost session " << sessionId << " detected, but not found from session list";
}

void SensorManager::dbusClientUnregistered(const QString &clientName)
{
    qCInfo(lcSensorFw) << "Watched D-Bus service '" << clientName << "' unregistered";
    QMap<int, SessionInstanceEntry*>::iterator it = sessionInstanceMap_.begin();
    while (it != sessionInstanceMap_.end()) {
        QMap<int, SessionInstanceEntry*>::iterator prev = it;
        ++it;
        if (prev.value()->m_clientName == clientName)
            lostClient(prev.key());
    }
}

void SensorManager::displayStateChanged(bool displayState)
{
    qCInfo(lcSensorFw) << "Signal detected, display state changed to:" << displayState;
    if (displayState) {
        /// Emit signal to make background calibration resume from sleep
        emit displayOn();
#ifdef SENSORFW_MCE_WATCHER
        if (!mceWatcher_->PSMEnabled())
#else
    #ifdef SENSORFW_LUNA_SERVICE_CLIENT
        if (!lsClient_->PSMEnabled())
    #endif // SENSORFW_LUNA_SERVICE_CLIENT
#endif // SENSORFW_MCE_WATCHER
        {
            emit resumeCalibration();
        }
    }

    foreach (const DeviceAdaptorInstanceEntry& adaptor, deviceAdaptorInstanceMap_) {
        if (adaptor.adaptor_) {
            if (displayState) {
                adaptor.adaptor_->setScreenBlanked(false);
                adaptor.adaptor_->resume();

            } else {
                adaptor.adaptor_->setScreenBlanked(true);
                adaptor.adaptor_->standby();
            }
        }
    }
}

void SensorManager::devicePSMStateChanged(bool psmState)
{
    if (psmState) {
        emit stopCalibration();
    }
}

QStringList SensorManager::printStatus() const
{
    QStringList output;
    output.append("  Adaptors:");
    for (QMap<QString, DeviceAdaptorInstanceEntry>::const_iterator it = deviceAdaptorInstanceMap_.constBegin();
         it != deviceAdaptorInstanceMap_.constEnd(); ++it) {
        output.append(QString("    %1 [%2 listener(s)] %3").arg(it.value().type_)
                      .arg(it.value().cnt_).arg(it.value().adaptor_->deviceStandbyOverride()
                                                ? "Standby Overriden" : "No standby override"));
    }

    output.append("  Chains:\n");
    for (QMap<QString, ChainInstanceEntry>::const_iterator it = chainInstanceMap_.constBegin();
         it != chainInstanceMap_.constEnd(); ++it) {
        output.append(QString("    %1 [%2 listener(s)]. %3")
                      .arg(it.value().type_).arg(it.value().cnt_).arg((it.value().chain_ && it.value().chain_->running())
                                                                      ? "Running" : "Stopped"));
    }

    output.append("  Logical sensors:");
    for (QMap<QString, SensorInstanceEntry>::const_iterator it = sensorInstanceMap_.constBegin();
         it != sensorInstanceMap_.constEnd(); ++it) {

        QString str;
        str.append(QString("    %1 [").arg(it.value().type_));
        if (it.value().sessions_.size())
            str.append(QString("%1 session(s), PID(s): %2]")
                       .arg(it.value().sessions_.size()).arg(socketToPid(it.value().sessions_)));
        else
            str.append("No sessions]");
        str.append(QString(". %1").arg((it.value().sensor_ && it.value().sensor_->running()) ? "Running" : "Stopped"));
        output.append(str);
    }

    return output;
}

QString SensorManager::socketToPid(int id) const
{
    struct ucred cr;
    socklen_t len = sizeof(cr);
    int fd = socketHandler_->getSocketFd(id);
    if (fd) {
        if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cr, &len) == 0)
            return QString("%1").arg(cr.pid);
        else
            return strerror(errno);
    }
    return "n/a";
}

QString SensorManager::socketToPid(const QSet<int>& ids) const
{
    QString str;
    bool first = true;
    foreach (int id, ids) {
        if (!first)
            str.append(", ");
        first = false;
        str.append(socketToPid(id));
    }
    return str;
}

int SensorManager::createNewSessionId()
{
    return ++sessionIdCount_;
}

const SensorInstanceEntry* SensorManager::getSensorInstance(const QString& id) const
{
    QMap<QString, SensorInstanceEntry>::const_iterator it(sensorInstanceMap_.find(id));
    if (it == sensorInstanceMap_.end()) {
        qCWarning(lcSensorFw) << "Failed to locate sensor instance: " << id;
        return nullptr;
    }
    return &it.value();
}

SensorManagerError SensorManager::errorCode() const
{
    return errorCode_;
}

int SensorManager::errorCodeInt() const
{
    return static_cast<int>(errorCode_);
}

const QString& SensorManager::errorString() const
{
    return errorString_;
}

void SensorManager::clearError()
{
    errorCode_ = SmNoError;
    errorString_.clear();
}

SocketHandler& SensorManager::socketHandler() const
{
    return *socketHandler_;
}

QList<QString> SensorManager::getAdaptorTypes() const
{
    return deviceAdaptorInstanceMap_.keys();
}

int SensorManager::getAdaptorCount(const QString& type) const
{
    QMap<QString, DeviceAdaptorInstanceEntry>::const_iterator it = deviceAdaptorInstanceMap_.find(type);
    if (it == deviceAdaptorInstanceMap_.end())
        return 0;
    return it.value().cnt_;
}

#ifdef SENSORFW_MCE_WATCHER
MceWatcher* SensorManager::MCEWatcher() const
{
    return mceWatcher_;
}
#endif // SENSORFW_MCE_WATCHER

#ifdef SENSORFW_LUNA_SERVICE_CLIENT
LSClient* SensorManager::LSClient_instance() const
{
    return lsClient_;
}
#endif // SENSORFW_LUNA_SERVICE_CLIENT

#ifdef SM_PRINT
void SensorManager::print() const
{
    qCInfo(lcSensorFw) << "Registry Dump:";
    foreach (const QString& id, sensorInstanceMap_.keys()) {
        qCInfo(lcSensorFw) << "Registry entry id  =" << id;

        qCInfo(lcSensorFw) << "sessions     =" << sensorInstanceMap_[id].sessions_;
        qCInfo(lcSensorFw) << "sensor             =" << sensorInstanceMap_[id].sensor_;
        qCInfo(lcSensorFw) << "type               =" << sensorInstanceMap_[id].type_ << endl;
    }

    qCInfo(lcSensorFw) << "sensorInstanceMap(" << sensorInstanceMap_.size() << "):" << sensorInstanceMap_.keys();
    qCInfo(lcSensorFw) << "sensorFactoryMap(" << sensorFactoryMap_.size() << "):" << sensorFactoryMap_.keys();
}
#endif

double SensorManager::magneticDeviation()
{
    if (deviation == 0) {
        QSettings confFile("/etc/xdg/sensorfw/location.conf", QSettings::IniFormat);
        confFile.beginGroup("location");
        deviation = confFile.value("declination",0).toDouble();
    }
    return deviation;
}

void SensorManager::setMagneticDeviation(double level)
{
    if (level != deviation) {
        QSettings confFile("/etc/xdg/sensorfw/location.conf", QSettings::IniFormat);
        confFile.beginGroup("location");
        confFile.setValue("declination",level);
        deviation = level;
    }
}
