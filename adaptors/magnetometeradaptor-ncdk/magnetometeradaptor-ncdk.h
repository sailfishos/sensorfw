/**
   @file magnetometeradaptor-ncdk.h
   @brief MagnetometerAdaptor for ncdk

   <p>
   Copyright (C) 2010-2011 Nokia Corporation

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
#ifndef MAGNETOMETERADAPTOR_NCDK_H
#define MAGNETOMETERADAPTOR_NCDK_H

#include "sysfsadaptor.h"
#include "deviceadaptorringbuffer.h"
#include "datatypes/genericdata.h"
#include <QString>
#include "datatypes/orientationdata.h"

class MagnetometerAdaptorNCDK : public SysfsAdaptor
{
    Q_OBJECT;
public:

    Q_PROPERTY(int overflowLimit READ overflowLimit WRITE setOverflowLimit);

    /**
     * Factory method for gaining a new instance of MagnetometerAdaptor class.
     * @param id Identifier for the adaptor.
     */
    static DeviceAdaptor* factoryMethod(const QString& id)
    {
        return new MagnetometerAdaptorNCDK(id);
    }

    bool startSensor();
    void stopSensor();

protected:
    /**
     * Constructor.
     * @param id Identifier for the adaptor.
     */
    MagnetometerAdaptorNCDK(const QString& id);
    ~MagnetometerAdaptorNCDK();

    bool setInterval(const int sessionId, const unsigned int interval_us);

private:

    /**
     * Read and process data. Run when sysfsadaptor has detected new available
     * data.
     * @param pathId PathId for the file that had event. Always 0, as we monitor
     *               only single file and don't set any proper id.
     * @param fd     Open file descriptor with new data. See #SysfsAdaptor::processSample()
     */
    void processSample(int pathId, int fd);

    QByteArray m_powerStateFilePath;
    QByteArray m_sensAdjFilePath;

    int m_x_adj;
    int m_y_adj;
    int m_z_adj;
    bool m_powerState;

    DeviceAdaptorRingBuffer<CalibratedMagneticFieldData> *m_magnetometerBuffer;

    bool setPowerState(bool value) const;
    void getSensitivityAdjustment(int &x, int &y, int &z) const;

    int adjustPos(const int value, const int adj) const;
    int m_intervalCompensation_us;
    int m_overflowLimit;

    /**
     * Sets the overflow limit of the sensor, checked when calibrated
     *
     * @param limit overflow limit.
     */
    void setOverflowLimit(int limit);

    /**
     * Get the overflow limit.
     *
     * @return overflow limit.
     */
    int overflowLimit() const;
};

#endif // MAGNETOMETERADAPTOR_NCDK_H
