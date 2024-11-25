/**
   @file sockethandler.cpp
   @brief SocketHandler

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

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

#include <QLocalSocket>
#include <QLocalServer>
#include <sys/socket.h>
#include <sys/time.h>
#include "logging.h"
#include "sockethandler.h"
#include <unistd.h>
#include <limits.h>

SessionData::SessionData(QLocalSocket* socket, QObject* parent)
    : QObject(parent),
      m_socket(socket),
      m_interval_us(-1),
      m_buffer(nullptr),
      m_size(0),
      m_count(0),
      m_bufferSize(1),
      m_bufferInterval_us(0),
      m_downsampling(false)
{
    m_lastWrite.tv_sec = 0;
    m_lastWrite.tv_usec = 0;
    m_timer.setSingleShot(true);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(timerTimeout()));
}

SessionData::~SessionData()
{
    m_timer.stop();
    delete m_socket;
    delete[] m_buffer;
}

void SessionData::timerTimeout()
{
    delayedWrite();
}

long SessionData::sinceLastWrite() const
{
    if (m_lastWrite.tv_sec == 0)
        return LONG_MAX;
    struct timeval now = {0, 0};
    gettimeofday(&now, 0);
    struct timeval diff;
    timersub(&now, &m_lastWrite, &diff);
    long interval_us = diff.tv_sec * 1000000L + diff.tv_usec;
    return interval_us;
}

bool SessionData::write(void* source, int size, unsigned int count)
{
    if (m_socket && count) {
        memcpy(source, &count, sizeof(unsigned int));
        int written = m_socket->write((const char*)source, size * count + sizeof(unsigned int));
        if (written < 0) {
            sensordLogW() << "[SocketHandler]: failed to write payload to the socket: " << m_socket->errorString();
            return false;
        }
        return true;
    }
    return false;
}

bool SessionData::write(const void* source, int size)
{
    long since_us = sinceLastWrite();
    int allocSize = m_bufferSize * size + sizeof(unsigned int);

    if (!m_buffer) {
        m_buffer = new char[allocSize];
    } else if (size != m_size) {
        m_socket->waitForBytesWritten();
        delete[] m_buffer;
        m_buffer = new char[allocSize];
    }

    m_size = size;
    if (m_bufferSize <= 1) {
        memcpy(m_buffer + sizeof(unsigned int), source, size);
        if (!m_downsampling || (m_downsampling && since_us >= m_interval_us)) {
            gettimeofday(&m_lastWrite, 0);
            return write(m_buffer, size, 1);
        }
    } else {
        memcpy(m_buffer + sizeof(unsigned int) + size * m_count, source, size);
        ++m_count;
        if (m_bufferSize == m_count) {
            return delayedWrite();
        }
    }

    if (!m_timer.isActive()) {
        if (m_bufferSize > 1 && m_bufferInterval_us) {
            int interval_ms = (m_bufferInterval_us + 999) / 1000;
            m_timer.start(interval_ms);
        } else if (!m_bufferSize && m_interval_us > since_us) {
            int interval_ms = (m_interval_us - since_us + 999) / 1000;
            m_timer.start(interval_ms);
        }
    }
    return true;
}

bool SessionData::delayedWrite()
{
    if (m_timer.isActive())
        m_timer.stop();
    gettimeofday(&m_lastWrite, 0);
    bool ret = write(m_buffer, m_size, m_count);
    m_count = 0;
    return ret;
}

QLocalSocket* SessionData::stealSocket()
{
    QLocalSocket *tmpsocket = m_socket;
    m_socket = nullptr;
    return tmpsocket;
}

QLocalSocket* SessionData::getSocket() const
{
    return m_socket;
}

void SessionData::setInterval(int interval_us)
{
    m_interval_us = interval_us;
}

int SessionData::getInterval() const
{
    return m_interval_us;
}

void SessionData::setBufferInterval(unsigned int interval_us)
{
    m_bufferInterval_us = interval_us;
}

unsigned int SessionData::getBufferInterval() const
{
    return m_bufferInterval_us;
}

void SessionData::setBufferSize(unsigned int size)
{
    if (size != m_bufferSize) {
        if (m_timer.isActive())
            m_timer.stop();
        m_socket->waitForBytesWritten();
        delete[] m_buffer;
        m_buffer = 0;
        m_count = 0;
        m_bufferSize = size;
        if (m_bufferSize < 1)
            m_bufferSize = 1;
        sensordLogT() << "[SocketHandler]: new buffersize: " << m_bufferSize;
    }
}

unsigned int SessionData::getBufferSize() const
{
    return m_bufferSize;
}

void SessionData::setDownsampling(bool value)
{
    if (value != m_downsampling) {
        m_downsampling = value;
        if (m_timer.isActive())
            m_timer.stop();
    }
}

bool SessionData::getDownsampling() const
{
    return m_downsampling;
}

SocketHandler::SocketHandler(QObject* parent) : QObject(parent), m_server(NULL)
{
    m_server = new QLocalServer(this);
    connect(m_server, SIGNAL(newConnection()), this, SLOT(newConnection()));
}

SocketHandler::~SocketHandler()
{
    delete m_server;
}

bool SocketHandler::listen(const QString& serverName)
{
    if (m_server->isListening()) {
        sensordLogW() << "[SocketHandler]: Already listening";
        return false;
    }

    bool unlinkDone = false;
    while (!m_server->listen(serverName) && !unlinkDone && serverName[0] == QChar('/')) {
        if (unlink(serverName.toLocal8Bit().constData()) == 0) {
            sensordLogD() << "[SocketHandler]: Unlinked stale socket" << serverName;
        } else {
            sensordLogD() << m_server->errorString();
        }
        unlinkDone = true;
    }
    return m_server->isListening();
}

bool SocketHandler::write(int id, const void* source, int size)
{
    QMap<int, SessionData*>::iterator it = m_idMap.find(id);
    if (it == m_idMap.end()) {
        sensordLogD() << "[SocketHandler]: Trying to write to nonexistent session (normal, no panic).";
        return false;
    }
    return (*it)->write(source, size);
}

bool SocketHandler::removeSession(int sessionId)
{
    if (!(m_idMap.keys().contains(sessionId))) {
        sensordLogW() << "[SocketHandler]: Trying to remove nonexistent session.";
        return false;
    }

    QLocalSocket* socket = (*m_idMap.find(sessionId))->stealSocket();

    if (socket) {
        disconnect(socket, SIGNAL(readyRead()), this, SLOT(socketReadable()));
        disconnect(socket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
        disconnect(socket, SIGNAL(error(QLocalSocket::LocalSocketError)),
                   this, SLOT(socketError(QLocalSocket::LocalSocketError)));
        socket->deleteLater();
    }

    delete m_idMap.take(sessionId);

    return true;
}

void SocketHandler::checkConnectionEstablished(int sessionId)
{
    if (!(m_idMap.keys().contains(sessionId))) {
        sensordLogW() << "[SocketHandler]: Socket connection for session" << sessionId
                      << "hasn't been estabilished. Considering session lost";
        emit lostSession(sessionId);
    }
}

void SocketHandler::newConnection()
{
    sensordLogT() << "[SocketHandler]: New connection received.";

    while (m_server->hasPendingConnections()) {

        QLocalSocket* socket = m_server->nextPendingConnection();
        connect(socket, SIGNAL(readyRead()), this, SLOT(socketReadable()));
        connect(socket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
        connect(socket, SIGNAL(error(QLocalSocket::LocalSocketError)),
                this, SLOT(socketError(QLocalSocket::LocalSocketError)));

        // Initialize socket
        socket->write("\n", 1);
        socket->waitForBytesWritten();
    }
}

void SocketHandler::socketReadable()
{
    int sessionId = -1;
    QLocalSocket* socket = (QLocalSocket*)sender();
    ((QLocalSocket*) sender())->read((char*)&sessionId, sizeof(int));

    disconnect(socket, SIGNAL(readyRead()), this, SLOT(socketReadable()));

    if (sessionId >= 0) {
        if (!m_idMap.contains(sessionId))
            m_idMap.insert(sessionId, new SessionData((QLocalSocket*)sender(), this));
    } else {
        sensordLogC() << "[SocketHandler]: Failed to read valid session ID from client. Closing socket.";
        socket->abort();
    }
}

void SocketHandler::socketDisconnected()
{
    QLocalSocket* socket = (QLocalSocket*)sender();

    int sessionId = -1;
    for (QMap<int, SessionData*>::const_iterator it = m_idMap.constBegin(); it != m_idMap.constEnd(); ++it) {
        if (it.value()->getSocket() == socket)
            sessionId = it.key();
    }

    if (sessionId == -1) {
        sensordLogW() << "[SocketHandler]: Noticed lost session, but can't find it.";
        return;
    }

    sensordLogW() << "[SocketHandler]: Noticed lost session: " << sessionId;
    emit lostSession(sessionId);
}

void SocketHandler::socketError(QLocalSocket::LocalSocketError socketError)
{
    sensordLogW() << "[SocketHandler]: Socket error: " << socketError;
    socketDisconnected();
}

int SocketHandler::getSocketFd(int sessionId) const
{
    QMap<int, SessionData*>::const_iterator it = m_idMap.find(sessionId);
    if (it != m_idMap.end() && (*it)->getSocket())
        return (*it)->getSocket()->socketDescriptor();
    return 0;
}

void SocketHandler::setInterval(int sessionId, int interval_us)
{
    QMap<int, SessionData*>::iterator it = m_idMap.find(sessionId);
    if (it != m_idMap.end())
        (*it)->setInterval(interval_us);
}

void SocketHandler::clearInterval(int sessionId)
{
    QMap<int, SessionData*>::iterator it = m_idMap.find(sessionId);
    if (it != m_idMap.end())
        (*it)->setInterval(-1);
}

int SocketHandler::interval(int sessionId) const
{
    QMap<int, SessionData*>::const_iterator it = m_idMap.find(sessionId);
    if (it != m_idMap.end())
        return (*it)->getInterval();
    return 0;
}

void SocketHandler::setBufferSize(int sessionId, unsigned int value)
{
    QMap<int, SessionData*>::iterator it = m_idMap.find(sessionId);
    if (it != m_idMap.end())
        (*it)->setBufferSize(value);
}

void SocketHandler::clearBufferSize(int sessionId)
{
    setBufferSize(sessionId, 0);
}

unsigned int SocketHandler::bufferSize(int sessionId) const
{
    QMap<int, SessionData*>::const_iterator it = m_idMap.find(sessionId);
    if (it != m_idMap.end())
        return (*it)->getBufferSize();
    return 0;
}

void SocketHandler::setBufferInterval(int sessionId, unsigned int interval_us)
{
    QMap<int, SessionData*>::iterator it = m_idMap.find(sessionId);
    if (it != m_idMap.end())
        (*it)->setBufferInterval(interval_us);
}

void SocketHandler::clearBufferInterval(int sessionId)
{
    setBufferInterval(sessionId, 0);
}

unsigned int SocketHandler::bufferInterval(int sessionId) const
{
    QMap<int, SessionData*>::const_iterator it = m_idMap.find(sessionId);
    if (it != m_idMap.end())
        return (*it)->getBufferInterval();
    return 0;
}

bool SocketHandler::downsampling(int sessionId) const
{
    QMap<int, SessionData*>::const_iterator it = m_idMap.find(sessionId);
    if (it != m_idMap.end())
        return (*it)->getBufferSize();
    return 0;
}

void SocketHandler::setDownsampling(int sessionId, bool value)
{
    QMap<int, SessionData*>::iterator it = m_idMap.find(sessionId);
    if (it != m_idMap.end())
        (*it)->setBufferInterval(value);
}
