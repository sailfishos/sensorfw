/*
 * SensorManager passes samples through a pipe that its own main thread reads.
 * The hybris adaptor writes to that pipe from the main thread too, so a full
 * pipe must never block the writer. Each test runs under an alarm: blocking
 * ends the run with SIGALRM rather than hanging it.
 *
 * Build against a built core and datatypes:
 *   qmake6 SENSORFW_CORE=<dir> SENSORFW_DT=<dir> sensormanagerpipe.pro
 */
#include <QByteArray>
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include <unistd.h>

#include "sensormanager.h"

/* Far more than the 5461 entries of a 64 KiB pipe. */
static const int overPipeCapacity = 20000;
static const int sessionId = 7;

class SensorManagerPipeTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir dir;

    /* Reads whole {count, sample} packets, returns the samples in order. */
    static bool readSamples(QLocalSocket &client, QList<int> &seen, int wanted)
    {
        QByteArray data;
        QElapsedTimer timer;
        timer.start();
        while (seen.size() < wanted && timer.elapsed() < 8000) {
            QCoreApplication::processEvents();
            client.waitForReadyRead(20);
            data += client.readAll();
            while (data.size() >= int(sizeof(unsigned) + sizeof(int))) {
                int value;
                memcpy(&value, data.constData() + sizeof(unsigned), sizeof(value));
                seen.append(value);
                data.remove(0, sizeof(unsigned) + sizeof(int));
            }
        }
        return seen.size() == wanted;
    }

    bool connectSession(QLocalSocket &client, int id)
    {
        client.connectToServer(dir.path() + "/run/sensord.sock");
        if (!client.waitForConnected(2000))
            return false;
        QCoreApplication::processEvents();
        if (!client.waitForReadyRead(2000))
            return false;
        client.readAll(); /* the "\n" greeting */
        client.write(reinterpret_cast<const char *>(&id), sizeof(id));
        client.flush();
        for (int i = 0; i < 20; ++i) {
            QCoreApplication::processEvents();
            QThread::msleep(5);
        }
        return true;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(dir.isValid());
        QVERIFY(QDir().mkpath(dir.path() + "/run"));
        qputenv("SENSORFW_SOCKET_PATH", dir.path().toUtf8());
        SensorManager::instance();
    }

    void init() { alarm(20); }
    void cleanup() { alarm(0); }

    void fullPipeOnTheMainThreadDoesNotBlock()
    {
        const int sample = 1;
        for (int i = 0; i < overPipeCapacity; ++i)
            QVERIFY(SensorManager::instance().write(9999, &sample, sizeof(sample)));
    }

    void noSampleIsLostWhenThePipeFills()
    {
        QLocalSocket client;
        QVERIFY(connectSession(client, sessionId));

        for (int i = 0; i < overPipeCapacity; ++i)
            QVERIFY(SensorManager::instance().write(sessionId, &i, sizeof(i)));

        QList<int> seen;
        QVERIFY(readSamples(client, seen, overPipeCapacity));
        for (int i = 0; i < overPipeCapacity; ++i)
            QCOMPARE(seen.at(i), i);
    }

    void aWriterOnAnotherThreadWaitsForRoom()
    {
        QLocalSocket client;
        QVERIFY(connectSession(client, sessionId + 1));

        const int id = sessionId + 1;
        QThread *writer = QThread::create([id] {
            for (int i = 0; i < overPipeCapacity; ++i)
                SensorManager::instance().write(id, &i, sizeof(i));
        });
        writer->start();

        QList<int> seen;
        QVERIFY(readSamples(client, seen, overPipeCapacity));
        QVERIFY(writer->wait(5000));
        delete writer;
        for (int i = 0; i < overPipeCapacity; ++i)
            QCOMPARE(seen.at(i), i);
    }

    void aClientLeavingDuringADrainIsHarmless()
    {
        auto *client = new QLocalSocket;
        QVERIFY(connectSession(*client, sessionId + 2));
        delete client; /* gone before the event loop has noticed */

        for (int i = 0; i < overPipeCapacity; ++i)
            QVERIFY(SensorManager::instance().write(sessionId + 2, &i, sizeof(i)));
        for (int i = 0; i < 50; ++i)
            QCoreApplication::processEvents();
    }

    void anEmptyPipeWakeupReturns()
    {
        /* Everything drained: the notifier's later wakeup finds nothing. */
        for (int i = 0; i < 10; ++i)
            QCoreApplication::processEvents();
        const int sample = 1;
        QVERIFY(SensorManager::instance().write(9999, &sample, sizeof(sample)));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
    }
};

QTEST_GUILESS_MAIN(SensorManagerPipeTest)
#include "sensormanagerpipetest.moc"
