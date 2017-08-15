/*
    Copyright 2009 Harald Fernengel <harry@kdevelop.org>
    Copyright 2017 René J.V. Bertin <rjvbertin@gmail.com>

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

#ifndef SOLID_BACKENDS_IOKIT_DEVICEINTERFACE_H
#define SOLID_BACKENDS_IOKIT_DEVICEINTERFACE_H

#include <solid/devices/ifaces/deviceinterface.h>
#include "iokitdevice.h"

#include <QtCore/QObject>
#include <QtCore/QStringList>

namespace Solid
{
namespace Backends
{
namespace IOKit
{
class DeviceInterface : public QObject, virtual public Solid::Ifaces::DeviceInterface
{
    Q_OBJECT
    Q_INTERFACES(Solid::Ifaces::DeviceInterface)
public:
    DeviceInterface(IOKitDevice *device);
    // the ctor taking a const device* argument makes a deep
    // copy of the IOKitDevice; any property changes made via 
    // the resulting instance will not affect the original device.
    DeviceInterface(const IOKitDevice *device);
    virtual ~DeviceInterface();

protected:
    IOKitDevice *m_device;
    IOKitDevice *copy;
};
}
}
}

#endif // SOLID_BACKENDS_IOKIT_DEVICEINTERFACE_H
