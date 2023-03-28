/**
   @file fakeadaptor.cpp
   @brief Fake adaptor for synthesizing input

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

#include <QtDebug>
#include <QFile>
#include "fakeadaptor.h"
#include <errno.h>
#include "datatypes/utils.h"

FakeAdaptor::FakeAdaptor(const QString &id) : DeviceAdaptor(id), m_interval_us(1000)
{
    m_thread = new FakeAdaptorThread(this);

    m_buffer = new DeviceAdaptorRingBuffer<TimedUnsigned>(1024);
    setAdaptedSensor("als", "Internal ambient light sensor lux values", m_buffer);
}

bool FakeAdaptor::startAdaptor()
{
    QFile file("/tmp/sensorTestSampleRate");

    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to get rate from" << file.fileName() << "- using 1000Hz (open)";
        return true;
    }

    m_interval_us = atoi(file.readLine().data()) * 1000;
    if (m_interval_us <= 0) {
        qDebug() << "Failed to get rate from" << file.fileName() << "- using 1000Hz (readline)";
        m_interval_us = 1000;
        return true;
    }

    file.close();
    return true;
}

void FakeAdaptor::stopAdaptor()
{
}

bool FakeAdaptor::startSensor()
{
    qDebug() << "Pushing fake ALS data with" << m_interval_us << " us interval";
    // Start pushing data
    m_thread->running = true;
    m_thread->start();
    return true;
}

void FakeAdaptor::stopSensor()
{
    // Stop pushing data
    m_thread->running = false;
    m_thread->wait();
    qDebug() << "sensor stopped";
}


void FakeAdaptor::pushNewData(int& data)
{
    TimedUnsigned *lux = m_buffer->nextSlot();

    lux->timestamp_ = Utils::getTimeStamp();
    lux->value_ = data;

    m_buffer->commit();
    m_buffer->wakeUpReaders();
}

void FakeAdaptor::init()
{
}

FakeAdaptorThread::FakeAdaptorThread(FakeAdaptor *parent) : running(false), m_parent(parent)
{
    qDebug() << "Data pusher for ALS";
}

void FakeAdaptorThread::run()
{
    int i = 0;
    while(running) {
        int interval_ms = (m_parent->m_interval_us + 999) / 1000;
        QThread::msleep(interval_ms);
        m_parent->pushNewData(i);
        i++;
    }
}
