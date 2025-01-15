/****************************************************************************
**
** Copyright (c) 2025 Jollyboys Ltd.
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

#ifndef WAKEUPSENSOR_I_H
#define WAKEUPSENSOR_I_H

#include <QtDBus/QtDBus>

#include "datatypes/unsigned.h"
#include "abstractsensor_i.h"

/** Client interface for accessing wakeup sensor.
 *
 * Provides signal on wakeup event
 *
 * Sensor value of one is used for reporting wakeups.
 */
class WakeupSensorChannelInterface : public AbstractSensorChannelInterface
{
    Q_OBJECT
    Q_DISABLE_COPY(WakeupSensorChannelInterface)
    Q_PROPERTY(Unsigned wakeup READ wakeup NOTIFY wakeupChanged)

public:
    /** Name of the D-Bus interface for this class.
     */
    static const char *staticInterfaceName;

    /** Create new instance of the class.
     *
     * @param id Sensor ID.
     * @param sessionId Session ID.
     *
     * @return Pointer to new instance of the class.
     */
    static AbstractSensorChannelInterface *factoryMethod(const QString &id, int sessionId);

    /** Get latest wakeup from sensor daemon.
     *
     * @return wakeup reading.
     */
    Unsigned wakeup();

    /** Constructor.
     *
     * @param path      path.
     * @param sessionId session ID.
     */
    WakeupSensorChannelInterface(const QString &path, int sessionId);

    /** Request an interface to the sensor.
     *
     * @param id sensor ID.
     *
     * @return Pointer to interface, or NULL on failure.
     */
    static WakeupSensorChannelInterface *interface(const QString &id);

protected:
    virtual bool dataReceivedImpl();

Q_SIGNALS:
    /** Sent when wakeup event is received.
     *
     * @param value dummy value.
     */
    void wakeupChanged(const Unsigned &value);
};

namespace local {
  typedef ::WakeupSensorChannelInterface WakeupSensor;
}
#endif // WAKEUPSENSOR_I_H
