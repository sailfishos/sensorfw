/**
   @file magcoordinatealignfilter.h
   @brief MagCoordinateAlignFilter

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
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

#ifndef MAGCOORDINATEALIGNFILTER_H
#define MAGCOORDINATEALIGNFILTER_H

#include "datatypes/orientationdata.h"
#include "filter.h"

/**
 * TMagMatrix holds a transformation matrix.
 */
class TMagMatrix
{
private:
    static const int DIM = 3;

public:
    TMagMatrix() {
        setMatrix((const double[DIM][DIM]){{1,0,0},{0,1,0},{0,0,1}});
    }
    TMagMatrix(const TMagMatrix& other) {
        setMatrix(other.data_);
    }
    TMagMatrix(double m[][DIM]) {
        setMatrix(m);
    }
    TMagMatrix &operator=(const TMagMatrix &other) {
        setMatrix(other.data_);
        return *this;
    }

    double get(int i, int j) const {
        if (i >= DIM || j >= DIM || i < 0 || j < 0) {
            qWarning("Index out of bounds");
            return 0;
        }
        return data_[i][j];
    };

    void setMatrix(const double m[DIM][DIM]) {
        memcpy(data_, m, sizeof(double[DIM][DIM]));
    }

    double data_[DIM][DIM];
};
Q_DECLARE_METATYPE(TMagMatrix)

/**
 * @brief Coordinate alignment filter.
 *
 * Performs three dimensional coordinate transformations.
 * Transformation is described by transformation matrix which is set through
 * \c TMatrix property. Matrix must be of size 3x3. Default TMatrix is
 * identity matrix.
 */
class MagCoordinateAlignFilter : public QObject, public Filter<CalibratedMagneticFieldData, MagCoordinateAlignFilter, CalibratedMagneticFieldData>
{
    Q_OBJECT
    Q_PROPERTY(TMagMatrix transMatrix READ matrix WRITE setMatrix)
public:

    /**
     * Factory method.
     * @return New MagCoordinateAlignFilter instance as FilterBase*.
     */
    static FilterBase* factoryMethod() {
        return new MagCoordinateAlignFilter;
    }

    const TMagMatrix& matrix() const { return matrix_; }

    void setMatrix(const TMagMatrix& matrix) { matrix_ = matrix; }

protected:
    /**
     * Constructor.
     */
    MagCoordinateAlignFilter();

private:
    void filter(unsigned, const CalibratedMagneticFieldData*);

    TMagMatrix matrix_;
};

#endif // MagCoordinateAlignFilter_H
