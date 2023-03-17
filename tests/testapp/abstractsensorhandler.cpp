/**
   @file abstractsensorhandler.cpp
   @brief abstract sensor handler

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

#include "abstractsensorhandler.h"
#include "config.h"
#include "logging.h"

AbstractSensorHandler::AbstractSensorHandler(const QString& sensorName, QObject *parent) :
    QThread(parent),
    m_sensorName(sensorName),
    m_interval_ms(100),
    m_bufferinterval_ms(0),
    m_standbyoverride(false),
    m_buffersize(0),
    m_dataCount(0),
    m_frameCount(0),
    m_downsample(false)
{
    if (SensorFrameworkConfig::configuration() != NULL)
    {
        m_interval_ms = SensorFrameworkConfig::configuration()->value(m_sensorName + "/interval", 100);
        m_bufferinterval_ms = SensorFrameworkConfig::configuration()->value(m_sensorName + "/bufferinterval", 0);
        m_standbyoverride = SensorFrameworkConfig::configuration()->value(m_sensorName + "/standbyoverride", false);
        m_buffersize = SensorFrameworkConfig::configuration()->value(m_sensorName + "/buffersize", 0);
        m_downsample = SensorFrameworkConfig::configuration()->value(m_sensorName + "/downsample", false);
    }
}

AbstractSensorHandler::~AbstractSensorHandler()
{
}

void AbstractSensorHandler::run()
{
    startClient();
    exec();
}
