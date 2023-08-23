/**
   @file sensormanager_i.cpp
   @brief Proxy class for interface for SensorManager

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Joep van Gassel <joep.van.gassel@nokia.com>
   @author Semi Malinen <semi.malinen@nokia.com
   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Antti Virtanen <antti.i.virtanen@nokia.com>
   @author Shenghua <ext-shenghua.1.liu@nokia.com>

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

#include "sensormanager_i.h"
#include <QAbstractSocket>

void __attribute__ ((constructor)) qtapi_init(void)
{
    qRegisterMetaType<QAbstractSocket::SocketState>("SocketState");
}

const char* LocalSensorManagerInterface::staticInterfaceName = "local.SensorManager";

LocalSensorManagerInterface::LocalSensorManagerInterface(const QString& service, const QString& path, const QDBusConnection& connection, QObject* parent) :
    QDBusAbstractInterface(service, path, staticInterfaceName, connection, parent)
{
}

LocalSensorManagerInterface::~LocalSensorManagerInterface()
{
}

SensorManagerError LocalSensorManagerInterface::errorCode()
{
    return static_cast<SensorManagerError>(errorCodeInt());
}

QString LocalSensorManagerInterface::errorString()
{
    QDBusReply<QString> reply = call(QDBus::Block, QLatin1String("errorString"));
    if(reply.isValid())
        return reply.value();
    return "Failed to fetch error string";
}

int LocalSensorManagerInterface::errorCodeInt()
{
    QDBusReply<int> reply = call(QDBus::Block, QLatin1String("errorCodeInt"));
    if(reply.isValid())
        return reply.value();
    return -1;
}

QDBusReply<bool> LocalSensorManagerInterface::loadPlugin(const QString& name)
{
    QList<QVariant> argumentList;
    argumentList << QVariant::fromValue(name);
    QDBusPendingReply <bool> reply = asyncCallWithArgumentList(QLatin1String("loadPlugin"), argumentList);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(loadPluginFinished(QDBusPendingCallWatcher*)));

    return reply;
}

void LocalSensorManagerInterface::loadPluginFinished(QDBusPendingCallWatcher *watch)
{
    watch->deleteLater();
    QDBusPendingReply<bool> reply = *watch;

    if(reply.isError()) {
        qDebug() << Q_FUNC_INFO  << reply.error().message();
        Q_EMIT errorSignal(errorCode());
    }
    Q_EMIT loadPluginFinished();
}

QDBusReply<int> LocalSensorManagerInterface::requestSensor(const QString& id)
{
    qint64 pid = QCoreApplication::applicationPid();
    QList<QVariant> argumentList;
    argumentList << QVariant::fromValue(id);
    argumentList << QVariant::fromValue(pid);
    QDBusPendingReply <int> reply = asyncCallWithArgumentList(QLatin1String("requestSensor"), argumentList);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(requestSensorFinished(QDBusPendingCallWatcher*)));
    return reply;
}

void LocalSensorManagerInterface::requestSensorFinished(QDBusPendingCallWatcher *watch)
{
    watch->deleteLater();
    QDBusPendingReply<int> reply = *watch;

    if(reply.isError()) {
        qDebug() << Q_FUNC_INFO  << reply.error().message();
        Q_EMIT errorSignal(errorCode());
    }
    Q_EMIT requestSensorFinished();
}

QDBusReply<bool> LocalSensorManagerInterface::releaseSensor(const QString& id, int sessionId)
{
    qint64 pid = QCoreApplication::applicationPid();
    QList<QVariant> argumentList;
    argumentList << QVariant::fromValue(id) << QVariant::fromValue(sessionId);
    argumentList << QVariant::fromValue(pid);
    QDBusPendingReply <bool> reply = asyncCallWithArgumentList(QLatin1String("releaseSensor"), argumentList);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            SLOT(releaseSensorFinished(QDBusPendingCallWatcher*)));
    return reply;
}

void LocalSensorManagerInterface::releaseSensorFinished(QDBusPendingCallWatcher *watch)
{
    watch->deleteLater();
    QDBusPendingReply<bool> reply = *watch;

    if(reply.isError()) {
        qDebug() << Q_FUNC_INFO  << reply.error().message();
        Q_EMIT errorSignal(errorCode());
    }
    Q_EMIT releaseSensorFinished();
}
