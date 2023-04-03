/**
   @file lsclient.cpp
   @brief LunaService signal utility

   <p>
   Copyright (C) 2009-2010 Nokia Corporation
   Copyright (C) 2015-2018 Nikolay Nizov

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Lihan Guo <ext-lihan.4.guo@nokia.com>
   @author Nikolay Nizov <nizovn@gmail.com>

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

#include "lsclient.h"
#include "JSONUtils.h"
#include "HostBase.h"
#include <json.h>

#define URI_SIGNAL_ADDMATCH "palm://com.palm.lunabus/signal/addmatch"
#define JSON_CHARGER_SIGNAL_ADDMATCH "{\"category\":\"/com/palm/power\",\"method\":\"chargerStatus\"}"
#define URI_DISPLAY_STATUS "palm://com.palm.display/control/status"
#define JSON_DISPLAY_SUBSCRIBE "{\"subscribe\":true}"

LSClient::LSClient(QObject* parent) : QObject(parent),
                                      displayState(true),
                                      powerSave(true)
{
    bool retVal;
    LSError lserror;
    LSErrorInit(&lserror);
    LSHandle *serviceHandle;

    retVal = LSRegister("com.nokia.SensorService", &serviceHandle, &lserror);
    if (!retVal) {
        goto error;
    }

    retVal = LSCall(serviceHandle, URI_SIGNAL_ADDMATCH, JSON_CHARGER_SIGNAL_ADDMATCH, LSClient::chargerCallback, this, NULL, &lserror);
    if (!retVal) {
        goto error;
    }

    retVal = LSCall(serviceHandle, URI_DISPLAY_STATUS, JSON_DISPLAY_SUBSCRIBE, LSClient::displayCallback, this, NULL, &lserror);
    if (!retVal) {
        goto error;
    }

    retVal = LSGmainAttach(serviceHandle, HostBase::instance()->mainLoop(), &lserror);
    if (!retVal) {
        goto error;
    }

    return;

error:
    if (LSErrorIsSet(&lserror)) {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }

    qDebug() << id() << "Unable to start service.";
}

bool LSClient::displayEnabled() const
{
    return displayState;
}

bool LSClient::PSMEnabled() const
{
    return powerSave;
}

bool LSClient::chargerCallback(LSHandle *sh, LSMessage *message, void *ctx)
{
    LSError lserror;
    LSErrorInit(&lserror);

    // {"type": string, "connected": boolean}
    VALIDATE_SCHEMA_AND_RETURN(sh,
                               message,
                               SCHEMA_2(REQUIRED(type, string), REQUIRED(connected, boolean)));

    LSClient* lsclient = (LSClient*)ctx;

    json_object* label = 0;
    json_object* root = 0;
    bool newState = true;
    const char* str = LSMessageGetPayload(message);
    if (!str)
            goto error;

    root = json_tokener_parse(str);
    if (!root || is_error(root))
            goto error;

    label = json_object_object_get(root, "connected");
    if (!label)
            goto error;

    newState = !json_object_get_boolean(label);

    if (lsclient->powerSave != newState)
    {
        lsclient->powerSave = newState;
        emit lsclient->devicePSMStateChanged(newState);
    }

error:

    if (root && !is_error(root))
        json_object_put(root);

    return true;
}

bool LSClient::displayCallback(LSHandle *sh, LSMessage *message, void *ctx)
{
    LSError lserror;
    LSErrorInit(&lserror);

    VALIDATE_SCHEMA_AND_RETURN(sh,
                               message,
                               SCHEMA_7(
                               REQUIRED(returnValue, boolean),
                               REQUIRED(event, string),
                               OPTIONAL(state, string),
                               OPTIONAL(timeout, integer),
                               OPTIONAL(blockDisplay, string),
                               OPTIONAL(active, boolean),
                               OPTIONAL(subscribed, boolean)
                               ));

    LSClient* lsclient = (LSClient*)ctx;

    json_object* root = 0;
    const char* value = 0;
    bool newState = true;
    bool ret = false;
    const char* str = LSMessageGetPayload(message);
    if (!str)
            goto error;

    root = json_tokener_parse(str);
    if (!root || is_error(root))
            goto error;

    ret = json_object_get_boolean(json_object_object_get(root, "returnValue"));
    if (!ret)
            goto error;

    value = json_object_get_string(json_object_object_get(root, "event"));
    if (!value || is_error(value))
            goto error;

    if (0 == strcmp (value, "displayOff"))
        newState = false;

    value = json_object_get_string(json_object_object_get(root, "state"));
    if (value && !is_error(value))
        if (0 == strcmp (value, "off"))
            newState = false;

    if (lsclient->displayState != newState)
    {
        lsclient->displayState = newState;
        emit lsclient->displayStateChanged(newState);
    }

error:

    if (root && !is_error(root))
        json_object_put(root);

    return true;
}
