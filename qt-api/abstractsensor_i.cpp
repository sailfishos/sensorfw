/**
   @file abstractsensor_i.cpp
   @brief Base class for sensor interface

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Joep van Gassel <joep.van.gassel@nokia.com>
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Antti Virtanen <antti.i.virtanen@nokia.com>
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

#include "sensormanagerinterface.h"
#include "abstractsensor_i.h"
#ifdef SENSORFW_MCE_WATCHER
#include "mcewatcher.h"
#endif
#ifdef SENSORFW_LUNA_SERVICE_CLIENT
#include "lsclient.h"
#endif

struct AbstractSensorChannelInterface::AbstractSensorChannelInterfaceImpl : public QDBusAbstractInterface
{
    AbstractSensorChannelInterfaceImpl(QObject* parent, int sessionId, const QString& path, const char* interfaceName);

    void dbusConnectNotify(const QMetaMethod& signal) { QDBusAbstractInterface::connectNotify(signal); }
    SensorError m_errorCode;
    QString m_errorString;
    int m_sessionId;
    int m_interval_us;
    unsigned int m_bufferInterval_ms;
    unsigned int m_bufferSize;
    SocketReader m_socketReader;
    bool m_running;
    bool m_standbyOverride;
    bool m_downsampling;
};

AbstractSensorChannelInterface::AbstractSensorChannelInterfaceImpl::AbstractSensorChannelInterfaceImpl(QObject* parent, int sessionId, const QString& path, const char* interfaceName) :
    QDBusAbstractInterface(SERVICE_NAME, path, interfaceName, QDBusConnection::systemBus(), 0),
    m_errorCode(SNoError),
    m_errorString(""),
    m_sessionId(sessionId),
    m_interval_us(0),
    m_bufferInterval_ms(0),
    m_bufferSize(1),
    m_socketReader(parent),
    m_running(false),
    m_standbyOverride(false),
    m_downsampling(true)
{
}

AbstractSensorChannelInterface::AbstractSensorChannelInterface(const QString& path, const char* interfaceName, int sessionId) :
    pimpl_(new AbstractSensorChannelInterfaceImpl(this, sessionId, path, interfaceName))
{
    if (!pimpl_->m_socketReader.initiateConnection(sessionId)) {
        setError(SClientSocketError, "Socket connection failed.");
    }
#ifdef SENSORFW_MCE_WATCHER
    MceWatcher *mcewatcher;
    mcewatcher = new MceWatcher(this);
    QObject::connect(mcewatcher,SIGNAL(displayStateChanged(bool)),
                     this,SLOT(displayStateChanged(bool)),Qt::UniqueConnection);
#endif
#ifdef SENSORFW_LUNA_SERVICE
    LSClient *lsclient;
    lsclient = new LSClient(this);
    QObject::connect(lsclient,SIGNAL(displayStateChanged(bool)),
                     this,SLOT(displayStateChanged(bool)),Qt::UniqueConnection);
#endif
}

AbstractSensorChannelInterface::~AbstractSensorChannelInterface()
{
    if ( pimpl_->isValid() )
        SensorManagerInterface::instance().releaseInterface(id(), pimpl_->m_sessionId);
    if (!pimpl_->m_socketReader.dropConnection())
        setError(SClientSocketError, "Socket disconnect failed.");
    delete pimpl_;
}

SocketReader& AbstractSensorChannelInterface::getSocketReader() const
{
    return pimpl_->m_socketReader;
}

bool AbstractSensorChannelInterface::release()
{
    return true;
}

void AbstractSensorChannelInterface::setError(SensorError errorCode, const QString& errorString)
{
    pimpl_->m_errorCode   = errorCode;
    pimpl_->m_errorString = errorString;
}

QDBusReply<void> AbstractSensorChannelInterface::start()
{
    return start(pimpl_->m_sessionId);
}

QDBusReply<void> AbstractSensorChannelInterface::stop()
{
    return stop(pimpl_->m_sessionId);
}

QDBusReply<void> AbstractSensorChannelInterface::start(int sessionId)
{
    clearError();

    if (pimpl_->m_running) {
        return QDBusReply<void>();
    }
    pimpl_->m_running = true;

    // Discard any old data already in the socket
    if (pimpl_->m_socketReader.socket()->bytesAvailable() > 0) {
        pimpl_->m_socketReader.socket()->readAll();
    }

    connect(pimpl_->m_socketReader.socket(), SIGNAL(readyRead()), this, SLOT(dataReceived()));

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId);

    QDBusPendingReply <void> returnValue = pimpl_->asyncCallWithArgumentList(QLatin1String("start"), argumentList);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(returnValue, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(startFinished(QDBusPendingCallWatcher*)));

    setStandbyOverride(sessionId, pimpl_->m_standbyOverride);
    setDataRate(sessionId, dataRate());
    setBufferInterval(sessionId, pimpl_->m_bufferInterval_ms);
    setBufferSize(sessionId, pimpl_->m_bufferSize);
    setDownsampling(pimpl_->m_sessionId, pimpl_->m_downsampling);

    return returnValue;
}

void AbstractSensorChannelInterface::startFinished(QDBusPendingCallWatcher *watch)
{
    watch->deleteLater();
    QDBusPendingReply<void> reply = *watch;

    if(reply.isError()) {
        qDebug() << reply.error().message();
        setError(SHwSensorStartFailed, reply.error().message());
    }
 }

QDBusReply<void> AbstractSensorChannelInterface::stop(int sessionId)
{
    clearError();
    if (!pimpl_->m_running) {
        return QDBusReply<void>();
    }
    pimpl_->m_running = false ;

    disconnect(pimpl_->m_socketReader.socket(), SIGNAL(readyRead()), this, SLOT(dataReceived()));

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId);

    QDBusPendingReply <void> returnValue = pimpl_->asyncCallWithArgumentList(QLatin1String("stop"), argumentList);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(returnValue, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(stopFinished(QDBusPendingCallWatcher*)));
    return returnValue;
}

void AbstractSensorChannelInterface::stopFinished(QDBusPendingCallWatcher *watch)
{
    watch->deleteLater();
    QDBusPendingReply<void> reply = *watch;

    if(reply.isError()) {
        qDebug() << reply.error().message();
        setError(SaCannotAccessSensor, reply.error().message());
    }
}

QDBusReply<void> AbstractSensorChannelInterface::setInterval(int sessionId, int interval_ms)
{
    clearError();

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId) << qVariantFromValue(interval_ms);
    QDBusPendingReply <void> returnValue = pimpl_->asyncCallWithArgumentList(QLatin1String("setInterval"), argumentList);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(returnValue, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(setIntervalFinished(QDBusPendingCallWatcher*)));
    return returnValue;
}

void AbstractSensorChannelInterface::setIntervalFinished(QDBusPendingCallWatcher *watch)
{
    watch->deleteLater();
    QDBusPendingReply<void> reply = *watch;

    if(reply.isError()) {
        qDebug() << reply.error().message();
        setError(SaCannotAccessSensor, reply.error().message());
    }
}

QDBusReply<void> AbstractSensorChannelInterface::setDataRate(int sessionId, double dataRate_Hz)
{
    clearError();

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId) << qVariantFromValue(dataRate_Hz);
    QDBusPendingReply <void> returnValue = pimpl_->asyncCallWithArgumentList(QLatin1String("setDataRate"), argumentList);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(returnValue, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(setDataRateFinished(QDBusPendingCallWatcher*)));
    return returnValue;
}

void AbstractSensorChannelInterface::setDataRateFinished(QDBusPendingCallWatcher *watch)
{
    watch->deleteLater();
    QDBusPendingReply<void> reply = *watch;

    if(reply.isError()) {
        qDebug() << reply.error().message();
        setError(SaCannotAccessSensor, reply.error().message());
    }
}

QDBusReply<void> AbstractSensorChannelInterface::setBufferInterval(int sessionId, unsigned int interval_ms)
{
    clearError();

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId) << qVariantFromValue(interval_ms);
    QDBusPendingReply <void> returnValue = pimpl_->asyncCallWithArgumentList(QLatin1String("setBufferInterval"), argumentList);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(returnValue, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(setBufferIntervalFinished(QDBusPendingCallWatcher*)));

    return returnValue;
}

void AbstractSensorChannelInterface::setBufferIntervalFinished(QDBusPendingCallWatcher *watch)
{
    watch->deleteLater();
    QDBusPendingReply<void> reply = *watch;

    if(reply.isError()) {
        qDebug() << reply.error().message();
        setError(SaCannotAccessSensor, reply.error().message());
    }
}

QDBusReply<void> AbstractSensorChannelInterface::setBufferSize(int sessionId, unsigned int value)
{
    clearError();

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId) << qVariantFromValue(value);

    QDBusPendingReply <void> returnValue = pimpl_->asyncCallWithArgumentList(QLatin1String("setBufferSize"), argumentList);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(returnValue, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(setBufferSizeFinished(QDBusPendingCallWatcher*)));
    return returnValue;
}

void AbstractSensorChannelInterface::setBufferSizeFinished(QDBusPendingCallWatcher *watch)
{
    watch->deleteLater();
    QDBusPendingReply<void> reply = *watch;

    if(reply.isError()) {
        qDebug() << reply.error().message();
        setError(SaCannotAccessSensor, reply.error().message());
    }
}

QDBusReply<bool> AbstractSensorChannelInterface::setStandbyOverride(int sessionId, bool value)
{
    clearError();

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId) << qVariantFromValue(value);
    QDBusPendingReply <void> returnValue = pimpl_->asyncCallWithArgumentList(QLatin1String("setStandbyOverride"), argumentList);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(returnValue, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(setStandbyOverrideFinished(QDBusPendingCallWatcher*)));
    return returnValue;
}

void AbstractSensorChannelInterface::setStandbyOverrideFinished(QDBusPendingCallWatcher *watch)
{
    watch->deleteLater();
    QDBusPendingReply<void> reply = *watch;

    if(reply.isError()) {
        qDebug() << reply.error().message();
        setError(SaCannotAccessSensor, reply.error().message());
    }
}

DataRangeList AbstractSensorChannelInterface::getAvailableDataRanges()
{
    return getAccessor<DataRangeList>("getAvailableDataRanges");
}

DataRange AbstractSensorChannelInterface::getCurrentDataRange()
{
    return getAccessor<DataRange>("getCurrentDataRange");
}

void AbstractSensorChannelInterface::requestDataRange(DataRange range)
{
    clearError();
    call(QDBus::NoBlock, QLatin1String("requestDataRange"), qVariantFromValue(pimpl_->m_sessionId), qVariantFromValue(range));
}

void AbstractSensorChannelInterface::removeDataRangeRequest()
{
    clearError();
    call(QDBus::NoBlock, QLatin1String("removeDataRangeRequest"), qVariantFromValue(pimpl_->m_sessionId));
}

DataRangeList AbstractSensorChannelInterface::getAvailableIntervals()
{
    return getAccessor<DataRangeList>("getAvailableIntervals");
}

IntegerRangeList AbstractSensorChannelInterface::getAvailableBufferIntervals()
{
    return getAccessor<IntegerRangeList>("getAvailableBufferIntervals");
}

IntegerRangeList AbstractSensorChannelInterface::getAvailableBufferSizes()
{
    return getAccessor<IntegerRangeList>("getAvailableBufferSizes");
}

bool AbstractSensorChannelInterface::hwBuffering()
{
    return getAccessor<bool>("hwBuffering");
}

int AbstractSensorChannelInterface::sessionId() const
{
    return pimpl_->m_sessionId;
}

SensorError AbstractSensorChannelInterface::errorCode()
{
    if (pimpl_->m_errorCode != SNoError) {
        return pimpl_->m_errorCode;
    }
    return static_cast<SensorError>(getAccessor<int>("errorCodeInt"));
}

QString AbstractSensorChannelInterface::errorString()
{
    if (pimpl_->m_errorCode != SNoError)
        return pimpl_->m_errorString;
    return getAccessor<QString>("errorString");
}

QString AbstractSensorChannelInterface::description()
{
    return getAccessor<QString>("description");
}

QString AbstractSensorChannelInterface::id()
{
    return getAccessor<QString>("id");
}

int AbstractSensorChannelInterface::interval()
{
    if (pimpl_->m_running)
        return static_cast<int>(getAccessor<unsigned int>("interval"));
    int interval_ms = 0;
    if (pimpl_->m_interval_us > 0)
        interval_ms = (pimpl_->m_interval_us + 999) / 1000;
    return interval_ms;
}

void AbstractSensorChannelInterface::setInterval(int interval_ms)
{
    int interval_us = 0;
    if (interval_ms > 0)
        interval_us = interval_ms * 1000;
    pimpl_->m_interval_us = interval_us;
    if (pimpl_->m_running)
        setDataRate(pimpl_->m_sessionId, dataRate());
}

double AbstractSensorChannelInterface::dataRate()
{
    double dataRate_Hz = 0;
    if (pimpl_->m_interval_us > 0)
        dataRate_Hz = 1000000.0 / pimpl_->m_interval_us;
    return dataRate_Hz;
}

void AbstractSensorChannelInterface::setDataRate(double dataRate_Hz)
{
    int interval_us = 0;
    if (dataRate_Hz > 0)
        interval_us = 1000000.0 / dataRate_Hz;
    pimpl_->m_interval_us = interval_us;
    if (pimpl_->m_running)
        setDataRate(pimpl_->m_sessionId, dataRate());
}

unsigned int AbstractSensorChannelInterface::bufferInterval()
{
    if (pimpl_->m_running)
        return getAccessor<unsigned int>("bufferInterval");
    return pimpl_->m_bufferInterval_ms;
}

void AbstractSensorChannelInterface::setBufferInterval(unsigned int interval_ms)
{
    pimpl_->m_bufferInterval_ms = interval_ms;
    if (pimpl_->m_running)
        setBufferInterval(pimpl_->m_sessionId, interval_ms);
}

unsigned int AbstractSensorChannelInterface::bufferSize()
{
    if (pimpl_->m_running)
        return getAccessor<unsigned int>("bufferSize");
    return pimpl_->m_bufferSize;
}

void AbstractSensorChannelInterface::setBufferSize(unsigned int value)
{
    pimpl_->m_bufferSize = value;
    if (pimpl_->m_running)
        setBufferSize(pimpl_->m_sessionId, value);
}

bool AbstractSensorChannelInterface::standbyOverride()
{
    if (pimpl_->m_running)
        return getAccessor<unsigned int>("standbyOverride");
    return pimpl_->m_standbyOverride;
}

bool AbstractSensorChannelInterface::setStandbyOverride(bool override)
{
    pimpl_->m_standbyOverride = override;
    return setStandbyOverride(pimpl_->m_sessionId, override);
}

QString AbstractSensorChannelInterface::type()
{
    return getAccessor<QString>("type");
}

void AbstractSensorChannelInterface::clearError()
{
    pimpl_->m_errorCode = SNoError;
    pimpl_->m_errorString.clear();
}

void AbstractSensorChannelInterface::dataReceived()
{
    do
    {
        if(!dataReceivedImpl())
            return;
    } while(pimpl_->m_socketReader.socket()->bytesAvailable());
}

bool AbstractSensorChannelInterface::read(void* buffer, int size)
{
    return pimpl_->m_socketReader.read(buffer, size);
}

bool AbstractSensorChannelInterface::setDataRangeIndex(int dataRangeIndex)
{
    clearError();
    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(pimpl_->m_sessionId) << qVariantFromValue(dataRangeIndex);

    QDBusPendingReply <bool> returnValue = pimpl_->asyncCallWithArgumentList(QLatin1String("setDataRangeIndex"), argumentList);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(returnValue, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(setDataRangeIndexFinished(QDBusPendingCallWatcher*)));
    return returnValue;
}

void AbstractSensorChannelInterface::setDataRangeIndexFinished(QDBusPendingCallWatcher *watch)
{
    watch->deleteLater();
    QDBusPendingReply<void> reply = *watch;

    if(reply.isError()) {
        qDebug() << reply.error().message();
        setError(SaCannotAccessSensor, reply.error().message());
    }
}

QDBusMessage AbstractSensorChannelInterface::call(QDBus::CallMode mode,
                                                  const QString& method,
                                                  const QVariant& arg1,
                                                  const QVariant& arg2,
                                                  const QVariant& arg3,
                                                  const QVariant& arg4,
                                                  const QVariant& arg5,
                                                  const QVariant& arg6,
                                                  const QVariant& arg7,
                                                  const QVariant& arg8)
{
    return pimpl_->call(mode, method, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}

QDBusMessage AbstractSensorChannelInterface::callWithArgumentList(QDBus::CallMode mode, const QString& method, const QList<QVariant>& args)
{
    return pimpl_->call(mode, method, args);
}

void AbstractSensorChannelInterface::dbusConnectNotify(const QMetaMethod& signal)
{
    pimpl_->dbusConnectNotify(signal);
}

bool AbstractSensorChannelInterface::isValid() const
{
    return pimpl_->isValid();
}

bool AbstractSensorChannelInterface::downsampling()
{
    return pimpl_->m_downsampling;
}

bool AbstractSensorChannelInterface::setDownsampling(bool value)
{
    pimpl_->m_downsampling = value;
    return setDownsampling(pimpl_->m_sessionId, value).isValid();
}

QDBusReply<void> AbstractSensorChannelInterface::setDownsampling(int sessionId, bool value)
{
    clearError();

    QList<QVariant> argumentList;
    argumentList << qVariantFromValue(sessionId) << qVariantFromValue(value);    
    QDBusPendingReply <void> returnValue = pimpl_->asyncCallWithArgumentList(QLatin1String("setDownsampling"), argumentList);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(returnValue, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(setDownsamplingFinished(QDBusPendingCallWatcher*)));
    return returnValue;
}

void AbstractSensorChannelInterface::setDownsamplingFinished(QDBusPendingCallWatcher *watch)
{
    watch->deleteLater();
    QDBusPendingReply<void> reply = *watch;

    if(reply.isError()) {
        qDebug() << reply.error().message();
        setError(SaCannotAccessSensor, reply.error().message());
    }
}

void AbstractSensorChannelInterface::displayStateChanged(bool displayState)
{
    if (!pimpl_->m_standbyOverride) {
        if (!displayState) {
            stop();
        } else {
            start();
        }
    }
}

