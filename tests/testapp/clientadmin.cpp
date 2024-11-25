/**
   @file clientadmin.cpp
   @brief base class for sensorhandlers

   <p>
   Copyright (C) 2011 Nokia Corporation

   @author Shenghua Liu <ext-shenghua.1.liu@nokia.com>

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

#include "clientadmin.h"
#include "config.h"
#include <stdlib.h>

#ifdef TARGET_LIB_qtapi
#include "sensorhandler_qtapi.h"
#endif

#ifdef TARGET_LIB_qmsystem2
#include "sensorhandler_qmsystem2.h"
#endif

#ifdef TARGET_LIB_qtmob
#include "sensorhandler_qtmob.h"
#endif

const char* ClientAdmin::CONFIG_FILE_PATH = "/usr/share/sensorfw-tests/testapp.conf";

ClientAdmin::ClientAdmin(const Parser& parser, QObject *parent) :
        QObject(parent),parser(parser), printer(0)
{
}

void ClientAdmin::init()
{
    if (parser.configFileInput())
    {
        QString defConfigFile = parser.configFilePath();
        if (SensorFrameworkConfig::loadConfig(defConfigFile, ""))
            qCDebug(lcSensorFw) << "SensorFrameworkConfig file is loading successfully.";
        else
        {
            qCWarning(lcSensorFw) << "SensorFrameworkConfig file error! Load using default path.";
            SensorFrameworkConfig::loadConfig(CONFIG_FILE_PATH, "");
        }
    } else {
        SensorFrameworkConfig::loadConfig(CONFIG_FILE_PATH, "");
    }

    if (SensorFrameworkConfig::configuration() == NULL)
    {
        qCCritical(lcSensorFw) << "Failed to load configuration. Aborting.";
        exit(EXIT_FAILURE);
    }

    if (!SensorHandler::init(SensorFrameworkConfig::configuration()->groups()))
    {
        qCCritical(lcSensorFw) << "Failed to initialize SensorHandler. Aborting.";
        exit(EXIT_FAILURE);
    }
}

void ClientAdmin::runClients()
{
    init();

    foreach (const QString& sensorName, SensorFrameworkConfig::configuration()->groups())
    {
        int count = SensorFrameworkConfig::configuration()->value<int>(sensorName + "/instances", 0);
        for(int i = 0; i < count; ++i)
        {
            SensorHandler* handler = new SensorHandler(sensorName, this);
            if (parser.singleThread()) {
                handler->startClient();
            } else {
                handler->start();
            }
            handlers << handler;
        }
    }

    if(parser.statInterval())
    {
        printer = new StatPrinter(handlers, parser.statInterval(), this);
    }

    qCInfo(lcSensorFw) << "Threads are waiting.";
}

ClientAdmin::~ClientAdmin()
{
    if(!parser.gracefulShutdown())
        return;
    qCInfo(lcSensorFw) << "Exiting...";

    delete printer;
    foreach(AbstractSensorHandler* handler, handlers)
    {
        if (parser.singleThread())
        {
            handler->stopClient();
        }else {
            handler->exit(0);
            handler->wait();
        }
        delete handler;
    }
    SensorFrameworkConfig::close();
}
