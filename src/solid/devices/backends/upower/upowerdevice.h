/*
    Copyright 2010 Michael Zanetti <mzanetti@kde.org>
    Copyright 2010 Lukas Tinkl <ltinkl@redhat.com>

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

#ifndef UPOWERDEVICE_H
#define UPOWERDEVICE_H

#include <ifaces/device.h>
#include <solid/deviceinterface.h>

#include <QtDBus/QDBusInterface>
#include <QtCore/QSet>

namespace Solid
{
namespace Backends
{
namespace UPower
{

class UPowerDevice : public Solid::Ifaces::Device
{
    Q_OBJECT
public:
    UPowerDevice(const QString &udi);
    virtual ~UPowerDevice();

    QObject *createDeviceInterface(const Solid::DeviceInterface::Type &type) Q_DECL_OVERRIDE;
    bool queryDeviceInterface(const Solid::DeviceInterface::Type &type) const Q_DECL_OVERRIDE;
    QString description() const Q_DECL_OVERRIDE;
    QStringList emblems() const Q_DECL_OVERRIDE;
    QString icon() const Q_DECL_OVERRIDE;
    QString product() const Q_DECL_OVERRIDE;
    QString vendor() const Q_DECL_OVERRIDE;
    QString udi() const Q_DECL_OVERRIDE;
    QString parentUdi() const Q_DECL_OVERRIDE;

    QVariant prop(const QString &key) const;
    bool propertyExists(const QString &key) const;
    QMap<QString, QVariant> allProperties() const;

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void onPropertiesChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps);
    void slotChanged();
    void login1Resuming(bool active);

private:
    QString batteryTechnology() const;
    mutable QDBusInterface m_device;
    QString m_udi;
    mutable QVariantMap m_cache;

    void checkCache(const QString &key) const;
};

}
}
}

#endif // UPOWERDEVICE_H
