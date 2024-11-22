/**
   @file declinationfilter.cpp
   @brief DeclinationFilter

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
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

#include <QSettings>
#include <QSettings>

#include "declinationfilter.h"
#include "config.h"
#include "logging.h"

const char *DeclinationFilter::s_declinationKey = "/system/osso/location/settings/magneticvariation";

DeclinationFilter::DeclinationFilter()
    : Filter<CompassData, DeclinationFilter, CompassData>(this, &DeclinationFilter::correct)
    , m_declinationCorrection(0)
    , m_lastUpdate_us(0)
{
    // XXX: multiplication order is a bit fishy, but: config = milliseconds, default is 1 hour?
    quint64 updateInterval_ms = SensorFrameworkConfig::configuration()->value<quint64>("compass/declination_update_interval",
                                                                                       1000 * 60 * 60);
    m_updateInterval_us = updateInterval_ms * 1000;
    loadSettings();
}

void DeclinationFilter::correct(unsigned, const CompassData* data)
{
    CompassData newOrientation(*data);
    if (newOrientation.timestamp_ - m_lastUpdate_us > m_updateInterval_us) {
        loadSettings();
        m_lastUpdate_us = newOrientation.timestamp_;
    }

    newOrientation.correctedDegrees_ = newOrientation.degrees_;
    if (m_declinationCorrection.loadAcquire() != 0) {
        newOrientation.correctedDegrees_ += m_declinationCorrection.loadAcquire();
        newOrientation.correctedDegrees_ %= 360;
//        sensordLogT() << "DeclinationFilter corrected degree " << newOrientation.degrees_ << " => " << newOrientation.correctedDegrees_ << ". Level: " << newOrientation.level_;
    }
    m_orientation = newOrientation;
    source_.propagate(1, &m_orientation);
}

void DeclinationFilter::loadSettings()
{
    QSettings confFile("/etc/xdg/sensorfw/location.conf", QSettings::IniFormat);
    confFile.beginGroup("location");
    double declination = confFile.value("declination", 0).toDouble();
    if (declination != 0) {
        m_declinationCorrection = declination;
    }
    sensordLogD() << "Fetched declination correction from GConf: " << m_declinationCorrection.loadAcquire();
}

int DeclinationFilter::declinationCorrection()
{
    loadSettings();
    return m_declinationCorrection.loadAcquire();
}
