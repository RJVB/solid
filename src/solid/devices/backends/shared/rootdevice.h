/*
    Copyright 2010 Mario Bensi <mbensi@ipsquad.net>

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

#ifndef SOLID_BACKENDS_SHARED_ROOT_DEVICE_H
#define SOLID_BACKENDS_SHARED_ROOT_DEVICE_H

#include <solid/devices/ifaces/device.h>

#include <QtCore/QStringList>

namespace Solid
{
namespace Backends
{
namespace Shared
{

class RootDevice : public Solid::Ifaces::Device
{
    Q_OBJECT

public:
    explicit RootDevice(const QString &udi, const QString &parentUdi = QString());

    virtual ~RootDevice();

    QString udi() const Q_DECL_OVERRIDE;
    QString parentUdi() const Q_DECL_OVERRIDE;

    QString vendor() const Q_DECL_OVERRIDE;
    void setVendor(const QString &vendor);

    QString product() const Q_DECL_OVERRIDE;
    void setProduct(const QString &product);

    QString icon() const Q_DECL_OVERRIDE;
    void setIcon(const QString &icon);

    QStringList emblems() const Q_DECL_OVERRIDE;
    void setEmblems(const QStringList &emblems);

    QString description() const Q_DECL_OVERRIDE;
    void setDescription(const QString &description);

    bool queryDeviceInterface(const Solid::DeviceInterface::Type &type) const Q_DECL_OVERRIDE;

    QObject *createDeviceInterface(const Solid::DeviceInterface::Type &type) Q_DECL_OVERRIDE;
private:
    QString m_udi;
    QString m_parentUdi;
    QString m_vendor;
    QString m_product;
    QString m_icon;
    QStringList m_emblems;
    QString m_description;
};

}
}
}
#endif
