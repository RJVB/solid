/*
    Copyright 2014 Alejandro Fiestas Olivares <afiestas@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SOLID_INHIBITION_H
#define SOLID_INHIBITION_H

#include <QObject>

#include "solid_export.h"

namespace Solid
{

class InhibitionHolderPrivate
{

};
/**
 * Holds an inhibition
 *
 * This object is returned by Power::InhibitionJob::inhibition and it
 * hols a reference to the inhibition that has been performed.
 *
 * Delete this object to release the inhibition
 */
class SOLID_EXPORT Inhibition : public QObject
{
    Q_OBJECT
public:
    virtual ~Inhibition() {delete d_ptr;};
protected:
    explicit Inhibition(QObject *parent = 0) : QObject(parent), d_ptr(new InhibitionHolderPrivate) {};

    InhibitionHolderPrivate *const d_ptr;
};
}

#endif //SOLID_INHIBITION_HOLDER_H