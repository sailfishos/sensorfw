/**
   @file abstractsensorhandler.h
   @brief base class for sensorhandlers

   <p>
   Copyright (C) 2011 Nokia Corporation

   @author Antti Virtanen <antti.i.virtanen@nokia.com>

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

#ifndef ABSTRACTSENSORHANDLER_H
#define ABSTRACTSENSORHANDLER_H

#include <QThread>
#include <QString>

class AbstractSensorHandler : public QThread
{
public:
    AbstractSensorHandler(const QString& sensorName, QObject *parent = 0);

    virtual ~AbstractSensorHandler();

    virtual void run();
    virtual bool startClient() = 0;
    virtual bool stopClient() = 0;

    int dataCount() const { return m_dataCount; }
    QString sensorName() const { return m_sensorName; }

protected:
    QString m_sensorName;
    int m_interval_ms;
    int m_bufferinterval_ms;
    bool m_standbyoverride;
    int m_buffersize;
    int m_dataCount;
    int m_frameCount;
    bool m_downsample;
};

#endif // ABSTRACTSENSORHANDLER_H
