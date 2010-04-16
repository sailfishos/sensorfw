/**
   @file powermanagementtests.cpp
   @brief Automatic tests for power management
   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>

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
#include "accelerometersensor_i.h"

#include "powermanagementtests.h"
#include <stdlib.h>
#include <QFile>
#include <QTimer>
#include <QProcess>

#define DISABLE_TKLOCK QProcess::execute("gconftool-2 --set /system/osso/dsm/locks/touchscreen_keypad_autolock_enabled --type=bool false");
#define OPEN_TKLOCK QProcess::execute("mcetool --set-tklock-mode=unlocked");
#define BLANK_SCREEN QProcess::execute("mcetool --blank-screen");
#define UNBLANK_SCREEN QProcess::execute("mcetool --unblank-screen");

int readPollInterval(QString path)
{
    QFile file(path);
    int returnValue = -1;

    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        char buf[16];
        file.readLine(buf, sizeof(buf));
        file.close();

        returnValue = atoi(buf);
    }

    return returnValue;
}

void PowerManagementTest::initTestCase()
{
    SensorManagerInterface& remoteSensorManager = SensorManagerInterface::instance();
    QVERIFY( remoteSensorManager.isValid() );

    // Load plugins (should test running depend on plug-in load result?)
    remoteSensorManager.loadPlugin("accelerometersensor");

    remoteSensorManager.registerSensorInterface<AccelerometerSensorChannelInterface>("accelerometersensor");

    DISABLE_TKLOCK;
    OPEN_TKLOCK;

    helper.setInputFile(accInputFile);
    helper.start();
}

void PowerManagementTest::init() {}
void PowerManagementTest::cleanup() {}
void PowerManagementTest::cleanupTestCase()
{
    helper.terminate();
    helper.wait();
}

void PowerManagementTest::testIntervalStartStop()
{
    UNBLANK_SCREEN;

    AccelerometerSensorChannelInterface* accOne;

    accOne = AccelerometerSensorChannelInterface::controlInterface("accelerometersensor");
    QVERIFY2((accOne != NULL && accOne->isValid()), accOne->errorString().toLatin1());

    QVERIFY(readPollInterval(accPollFile) == 0);

    accOne->setInterval(100);

    QVERIFY(readPollInterval(accPollFile) == 0);

    accOne->start();

    QVERIFY(readPollInterval(accPollFile) == 100);

    accOne->stop();

    QVERIFY(readPollInterval(accPollFile) == 0);

    delete accOne;
}

void PowerManagementTest::testIntervalRace()
{
    UNBLANK_SCREEN;

    AccelerometerSensorChannelInterface* accOne;
    AccelerometerSensorChannelInterface* accTwo;
    QFile pollFile;

    accOne = AccelerometerSensorChannelInterface::controlInterface("accelerometersensor");
    QVERIFY2((accOne != NULL && accOne->isValid()), accOne->errorString().toLatin1());

    accTwo = const_cast<AccelerometerSensorChannelInterface*>(AccelerometerSensorChannelInterface::listenInterface("accelerometersensor"));
    QVERIFY2((accTwo != NULL && accTwo->isValid()), accTwo->errorString().toLatin1());

    QVERIFY(readPollInterval(accPollFile) == 0);

    accOne->setInterval(100);
    accOne->start();

    QVERIFY(readPollInterval(accPollFile) == 100);

    accTwo->setInterval(50);
    accTwo->start();

    QVERIFY(readPollInterval(accPollFile) == 50);

    accTwo->stop();

    QVERIFY(readPollInterval(accPollFile) == 100);

    accTwo->setInterval(150);
    accTwo->start();

    QVERIFY(readPollInterval(accPollFile) == 100);

    accOne->stop();

    QVERIFY(readPollInterval(accPollFile) == 150);

    accTwo->stop();
    
    QVERIFY(readPollInterval(accPollFile) == 0);

    delete accOne;
    delete accTwo;
}

/**
 * Verify normal screen blank behavior.
 */
void PowerManagementTest::testScreenBlank1()
{
    // Unblank to make sure we notice going into blank
    UNBLANK_SCREEN;

    AccelerometerSensorChannelInterface* accOne;

    accOne = AccelerometerSensorChannelInterface::controlInterface("accelerometersensor");
    QVERIFY2((accOne != NULL && accOne->isValid()), accOne->errorString().toLatin1());

    connect(accOne, SIGNAL(dataAvailable(const XYZ&)), &helper, SLOT(dataAvailable(const XYZ&)));

    accOne->start();

    QTest::qWait(1000);
    QVERIFY2(helper.m_valueCount > 0, "No samples received.");

    // Blank screen
    BLANK_SCREEN;
    QTest::qWait(500);
    helper.reset(); // Clear the buffer
    QTest::qWait(1000);

    // Verify that values have not come through
    QVERIFY2(helper.m_valueCount == 0, "Samples leaking through");

    // Unblank
    UNBLANK_SCREEN;
    QTest::qWait(500);

    // Reset - in testing the old value might stay in the fakepipe.
    helper.reset();

    // Verity that values comes through again
    QTest::qWait(1000);
    QVERIFY(helper.m_valueCount > 0);

    accOne->stop();

    delete accOne;
}

/**
 * Verify stopping sensor while screen is blanked.
 */
void PowerManagementTest::testScreenBlank2()
{
    // Unblank to make sure we notice going into blank
    UNBLANK_SCREEN;

    AccelerometerSensorChannelInterface* accOne;

    accOne = AccelerometerSensorChannelInterface::controlInterface("accelerometersensor");
    QVERIFY2((accOne != NULL && accOne->isValid()), accOne->errorString().toLatin1());

    connect(accOne, SIGNAL(dataAvailable(const XYZ&)), &helper, SLOT(dataAvailable(const XYZ&)));

    accOne->start();

    QTest::qWait(1000);
    QVERIFY2(helper.m_valueCount > 0, "No samples received.");

    // Blank screen
    BLANK_SCREEN;
    QTest::qWait(500);
    helper.reset(); // Clear the buffer
    QTest::qWait(1000);

    // Stop the sensor
    accOne->stop();

    // Verify that values have not come through
    QVERIFY2(helper.m_valueCount == 0, "Samples leaking through");

    // Unblank
    UNBLANK_SCREEN;
    QTest::qWait(500);

    // Reset - in testing the old value might stay in the fakepipe.
    helper.reset();

    // Verity that sensor was not incorrectly resumed
    QTest::qWait(1000);
    QVERIFY(helper.m_valueCount == 0);

    delete accOne;
}

/**
 * Verify starting sensor while screen is blanked.
 */
void PowerManagementTest::testScreenBlank3()
{
    // Unblank to make sure we notice going into blank
    UNBLANK_SCREEN;
    QTest::qWait(1000);

    AccelerometerSensorChannelInterface* accOne;

    accOne = AccelerometerSensorChannelInterface::controlInterface("accelerometersensor");
    QVERIFY2((accOne != NULL && accOne->isValid()), accOne->errorString().toLatin1());

    connect(accOne, SIGNAL(dataAvailable(const XYZ&)), &helper, SLOT(dataAvailable(const XYZ&)));

    // Blank screen
    BLANK_SCREEN;
    QTest::qWait(1000);
    helper.reset(); // Clear the buffer

    // Start the sensor
    accOne->start();

    QTest::qWait(1000);

    // Verify that values have not come through
    QVERIFY2(helper.m_valueCount == 0, "Samples leaking through");

    // Unblank
    UNBLANK_SCREEN;
    QTest::qWait(1000);

    // Verify that sensor was correctly resumed
    QTest::qWait(2000);
    QVERIFY2(helper.m_valueCount > 0, "Not receiving samples after resume");

    // Stop the sensor
    accOne->stop();

    delete accOne;
}

/**
 * Verify screen blank override behavior.
 */
void PowerManagementTest::testScreenBlank4()
{
    // Unblank to make sure we notice going into blank
    UNBLANK_SCREEN;
    QTest::qWait(1000);

    AccelerometerSensorChannelInterface* accOne;

    accOne = AccelerometerSensorChannelInterface::controlInterface("accelerometersensor");
    QVERIFY2((accOne != NULL && accOne->isValid()), accOne->errorString().toLatin1());

    connect(accOne, SIGNAL(dataAvailable(const XYZ&)), &helper, SLOT(dataAvailable(const XYZ&)));

    accOne->setStandbyOverride(true);
    accOne->start();

    QTest::qWait(1000);
    QVERIFY2(helper.m_valueCount > 0, "No samples received.");

    // Blank screen
    BLANK_SCREEN;
    QTest::qWait(1000);
    helper.reset(); // Clear the buffer
    QTest::qWait(1000);

    // Verify that values have not come through
    QVERIFY2(helper.m_valueCount >  0, "Adaptor apparently went to standby against rules");

    // Unblank
    UNBLANK_SCREEN;
    QTest::qWait(1000);

    // Reset - in testing the old value might stay in the fakepipe.
    helper.reset();

    // Verity that values comes through again
    QTest::qWait(1000);
    QVERIFY2(helper.m_valueCount > 0, "Not getting values after resume.");

    accOne->stop();

    delete accOne;
}

QTEST_MAIN(PowerManagementTest)