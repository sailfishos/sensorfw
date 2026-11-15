/**
   @file sockethandlertest.cpp
   @brief Automatic tests for SocketHandler
   <p>
   Copyright (C) 2026 Herman van Hazendonk

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

#include <QtTest>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "sockethandler.h"

class SocketHandlerTest : public QObject
{
    Q_OBJECT

private slots:
    void socketErrorIsHandled();
};

/*
 * A client that goes away must reach SocketHandler::socketError() through
 * the local socket's error signal, on every Qt version. The signal was
 * renamed to errorOccurred() in Qt 5.15 and the old name is gone in Qt 6,
 * where a connect by name only printed a warning and the error was never
 * handled.
 */
void SocketHandlerTest::socketErrorIsHandled()
{
    const QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString name = dir.path() + QStringLiteral("/sensord.sock");

    SocketHandler handler;
    QVERIFY(handler.listen(name));
    QSignalSpy lost(&handler, SIGNAL(lostSession(int)));
    // As SensorManager does: a lost session is released, which removes it
    // from the handler and disconnects its socket.
    connect(&handler, &SocketHandler::lostSession,
            &handler, [&handler](int id) { handler.removeSession(id); });

    // The handshake the sensorfw client library does: the server greets the
    // new connection with a newline, the client answers with its session id.
    const int sessionId = 7;
    QLocalSocket client;
    client.connectToServer(name);
    QVERIFY(client.waitForConnected(5000));
    QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 5000);
    QCOMPARE(client.read(1), QByteArray("\n"));
    QCOMPARE(client.write(reinterpret_cast<const char *>(&sessionId), sizeof(sessionId)),
             qint64(sizeof(sessionId)));
    QVERIFY(client.waitForBytesWritten(5000));
    QTRY_VERIFY_WITH_TIMEOUT(handler.getSocketFd(sessionId) != 0, 5000);

    // Only socketError() logs this; the server end of the connection reports
    // QLocalSocket::PeerClosedError once the client has gone.
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral("^\\[SocketHandler\\]: Socket error: ")));
    client.close();

    QTRY_VERIFY_WITH_TIMEOUT(lost.count() > 0, 5000);
    QCOMPARE(lost.first().first().toInt(), sessionId);
    QTRY_COMPARE_WITH_TIMEOUT(handler.getSocketFd(sessionId), 0, 5000);
    QCOMPARE(lost.count(), 1);
}

QTEST_MAIN(SocketHandlerTest)
#include "sockethandlertest.moc"
