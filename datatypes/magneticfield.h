/**
   @file magneticfield.h
   @brief QObject based datatype for CalibratedMagneticFieldData

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Tapio Rantala <ext-tapio.rantala@nokia.com>

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

#ifndef MAGNETICFIELDDATA_H
#define MAGNETICFIELDDATA_H

#include <QDBusArgument>
#include <datatypes/orientationdata.h>

/**
 * QObject facade for #CalibratedMagneticFieldData.
 */
class MagneticField : public QObject
{
public:
    Q_OBJECT;

public:

    /**
     * Default constructor.
     */
    MagneticField() : QObject() {}

    /**
     * Constructor.
     *
     * @param calibratedData Source object.
     */
    MagneticField(const CalibratedMagneticFieldData& calibratedData) : QObject() {
        data_.timestamp_ = calibratedData.timestamp_;
        data_.level_ = calibratedData.level_;
        data_.x_ = calibratedData.x_;
        data_.y_ = calibratedData.y_;
        data_.z_ = calibratedData.z_;
        data_.rx_ = calibratedData.rx_;
        data_.ry_ = calibratedData.ry_;
        data_.rz_ = calibratedData.rz_;
    }

    /**
     * Copy constructor.
     *
     * @param data Source object.
     */
    MagneticField(const MagneticField& data) : QObject() {
        data_.timestamp_ = data.data_.timestamp_;
        data_.level_ = data.data_.level_;
        data_.x_ = data.data_.x_;
        data_.y_ = data.data_.y_;
        data_.z_ = data.data_.z_;
        data_.rx_ = data.data_.rx_;
        data_.ry_ = data.data_.ry_;
        data_.rz_ = data.data_.rz_;
    }

    /**
     * Accessor for contained #CalibratedMagneticFieldData.
     *
     * @return contained #CalibratedMagneticFieldData.
     */
    const CalibratedMagneticFieldData& data() const { return data_; }

    /**
     * Assignment operator.
     *
     * @param origin Source object for assigment.
     */
    MagneticField& operator=(const MagneticField& origin)
    {
        data_.timestamp_ = origin.data_.timestamp_;
        data_.level_ = origin.data_.level_;
        data_.x_ = origin.data_.x_;
        data_.y_ = origin.data_.y_;
        data_.z_ = origin.data_.z_;
        data_.rx_ = origin.data_.rx_;
        data_.ry_ = origin.data_.ry_;
        data_.rz_ = origin.data_.rz_;

        return *this;
    }

    /**
     * Comparison operator.
     *
     * @param right Object to compare to.
     * @return comparison result.
     */
    bool operator==(const MagneticField& right) const
    {
        CalibratedMagneticFieldData rdata = right.data();
        return (data_.x_ == rdata.x_ &&
                data_.y_ == rdata.y_ &&
                data_.z_ == rdata.z_ &&
                data_.rx_ == rdata.rx_ &&
                data_.ry_ == rdata.ry_ &&
                data_.rz_ == rdata.rz_ &&
                data_.level_ == rdata.level_ &&
                data_.timestamp_ == rdata.timestamp_);
    }

    /**
     * Returns the value for X.
     * @return x value.
     */
    float x() const { return data_.x_; }

    /**
     * Returns the value for Y.
     * @return y value.
     */
    float y() const { return data_.y_; }

    /**
     * Returns the value for Z.
     * @return z value.
     */
    float z() const { return data_.z_; }

     /**
     * Returns the raw value for X.
     * @return raw x value.
     */
    float rx() const { return data_.rx_; }

    /**
     * Returns the raw value for Y.
     * @return raw y value.
     */
    float ry() const { return data_.ry_; }

    /**
     * Returns the raw value for Z.
     * @return raw z value.
     */
    float rz() const { return data_.rz_; }

    /**
     * Returns the magnetometer calibration level.
     * @return level of magnetometer calibration.
     */
    int level() const { return data_.level_; }

    /**
     * Returns the timestamp of sample as monotonic time (microsec).
     * @return timestamp value.
     */
    const quint64& timestamp() const { return data_.timestamp_; }

private:
    CalibratedMagneticFieldData data_; /**< Contained data */

    friend const QDBusArgument &operator>>(const QDBusArgument &argument, MagneticField& data);
};

Q_DECLARE_METATYPE( MagneticField )

/**
 * Marshall the MagneticField data into a D-Bus argument
 *
 * @param argument dbus argument.
 * @param data data to marshall.
 * @return dbus argument.
 */
inline QDBusArgument &operator<<(QDBusArgument &argument, const MagneticField &data)
{
    // No floats on D-Bus: Implicit float to double conversion
    argument.beginStructure();
    argument << data.data().timestamp_ << data.data().level_;
    argument << data.data().x_ << data.data().y_ << data.data().z_;
    argument << data.data().rx_ << data.data().ry_ << data.data().rz_;
    argument.endStructure();
    return argument;
}

/**
 * Unmarshall MagneticField data from the D-Bus argument
 *
 * @param argument dbus argument.
 * @param data unmarshalled data.
 * @return dbus argument.
 */
inline const QDBusArgument &operator>>(const QDBusArgument &argument, MagneticField &data)
{
    // No floats on D-Bus: Explicit double to float conversion
    argument.beginStructure();
    argument >> data.data_.timestamp_ >> data.data_.level_;
    double x, y, z;
    argument >> x >> y >> z;
    data.data_.x_ = float(x);
    data.data_.y_ = float(y);
    data.data_.z_ = float(z);
    argument >> x >> y >> z;
    data.data_.rx_ = float(x);
    data.data_.ry_ = float(y);
    data.data_.rz_ = float(z);
    argument.endStructure();
    return argument;
}

#endif // MAGNETICFIELDDATA_H
