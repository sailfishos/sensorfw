/****************************************************************************
**
** Copyright (c) 2013 Jolla Ltd.
** Copyright (c) 2025 Jollyboys Ltd.
** Copyright (c) 2026 Jolla Mobile Ltd
**
**
** $QT_BEGIN_LICENSE:LGPL$
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef HYBRISBACKEND_BINDER_HIDL_H
#define HYBRISBACKEND_BINDER_HIDL_H

#include "hybrisbackend_binder.h"

class HybrisBackendBinderHidl : public HybrisBackendBinder
{
public:
    HybrisBackendBinderHidl(HybrisManager *manager);
    ~HybrisBackendBinderHidl();
    static bool isSupported();
    void cleanup();
    void initialize() override;
    bool needsReaderThread() override;
    int setActive(int handle, bool active) override;
    int setDelay(int handle, int64_t delay_ns) override;
    void readEvents() override;

protected:
    static GBinderLocalReply *sensorCallbackHandler(
        GBinderLocalObject* obj,
        GBinderRemoteRequest* req,
        guint code,
        guint flags,
        int* status,
        void* user_data);
    void getSensorList();
    void startConnect();
    void finishConnect();
    static void binderDied(GBinderRemoteObject *, void *user_data);
    void pollEvents();
    static void pollEventsCallback(
        GBinderClient* /*client*/, GBinderRemoteReply* reply,
        int status, void* userData);

private:
    gulong m_pollTransactId;
};

#endif // HYBRISBACKEND_BINDER_HIDL_H
