/*
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

#include "iokitstorage.h"

#include <QDebug>

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>

using namespace Solid::Backends::IOKit;

class IOKitStorage::Private
{
public:
    Private(const IOKitDevice *device)
        : m_device(device)
    {
        daRef = 0;
        daDict = 0;
        daSession = DASessionCreate(kCFAllocatorDefault);
        if (daSession) {
            const QString devName = m_device->property(QStringLiteral("BSD Name")).toString();
            daRef = DADiskCreateFromBSDName(kCFAllocatorDefault, daSession, devName.toStdString().c_str());
            if (daRef) {
                daDict = DADiskCopyDescription(daRef);
            }
        }
    }
    virtual ~Private()
    {
        if (daDict) {
            CFRelease(daDict);
            daDict = 0;
        }
        if (daRef) {
            CFRelease(daRef);
            daRef = 0;
        }
        if (daSession) {
            CFRelease(daSession);
            daSession = 0;
        }
    }

    const IOKitDevice *m_device;
    DASessionRef daSession;
    DADiskRef daRef;
    CFDictionaryRef daDict;
};

IOKitStorage::IOKitStorage(IOKitDevice *device)
    : Block(device)
    , d(new Private(device))
{
}

IOKitStorage::IOKitStorage(const IOKitDevice *device)
    : Block(device)
    , d(new Private(device))
{
}

IOKitStorage::~IOKitStorage()
{
    delete d;
}

Solid::StorageDrive::Bus IOKitStorage::bus() const
{
//     if (d->daDict) {
//         qWarning() << Q_FUNC_INFO;
//         CFShow(d->daDict);
//     }

    const QString udi = m_device->udi();
    // TODO: figure out how to return something useful here.
    if (udi.contains(QStringLiteral("/SATA@"))) {
        return Solid::StorageDrive::Sata;
    }
    if (udi.contains(QStringLiteral("/SDXC@"))) {
        // TODO: return something finer grained; the built-in card reader
        // is NOT connected via USB on Macs, for instance (but there's no PCI option)
        return Solid::StorageDrive::Usb;
    }
    if (udi.contains(QStringLiteral("/IOUSBInterface@"))) {
        return Solid::StorageDrive::Usb;
    }
    if (d->daDict) {
        const CFStringRef devProtocol = (const CFStringRef) CFDictionaryGetValue(d->daDict,
             kDADiskDescriptionDeviceProtocolKey);
        if (QString::fromCFString(devProtocol) == QStringLiteral("USB")) {
            return Solid::StorageDrive::Usb;
        }
    }
    return Solid::StorageDrive::Platform;
}

Solid::StorageDrive::DriveType IOKitStorage::driveType() const
{
    const QString udi = m_device->udi();
    const QString type = m_device->property(QLatin1String("className")).toString();

    if (type == QStringLiteral("IOCDMedia")) {
        return Solid::StorageDrive::CdromDrive;
    }
    if (udi.contains(QStringLiteral("/SDXC@"))) {
        return Solid::StorageDrive::SdMmc;
    }
    if (d->daDict) {
        const CFStringRef devModel = (const CFStringRef) CFDictionaryGetValue(d->daDict,
             kDADiskDescriptionDeviceModelKey);
        if (QString::fromCFString(devModel) == QStringLiteral("Compact Flash")) {
            return Solid::StorageDrive::CompactFlash;
        }
    }
    return Solid::StorageDrive::HardDisk;
}

bool IOKitStorage::isRemovable() const
{
    bool isExternal = false;
    if (d->daDict) {
        const CFBooleanRef isInternal = (const CFBooleanRef) CFDictionaryGetValue(d->daDict,
            kDADiskDescriptionDeviceInternalKey);
        if (isInternal) {
            isExternal = !CFBooleanGetValue(isInternal);
        }
    }
    return isExternal || m_device->property(QLatin1String("Removable")).toBool();
}

bool IOKitStorage::isHotpluggable() const
{
    const Solid::StorageDrive::DriveType type = driveType();
    return bus() == Solid::StorageDrive::Usb
        || type == Solid::StorageDrive::CdromDrive || type == Solid::StorageDrive::SdMmc;
}

qulonglong IOKitStorage::size() const
{
    return m_device->property(QLatin1String("Size")).toULongLong();
}

const QString IOKitStorage::vendor()
{
    if (d->daDict) {
        return QString::fromCFString((const CFStringRef) CFDictionaryGetValue(d->daDict,
             kDADiskDescriptionDeviceVendorKey));
    }
    return QString();
}

const QString IOKitStorage::product()
{
    if (d->daDict) {
        return QString::fromCFString((const CFStringRef) CFDictionaryGetValue(d->daDict,
             kDADiskDescriptionDeviceModelKey));
    }
    return QString();
}

const QString IOKitStorage::description()
{
    if (d->daDict) {
        return QString::fromCFString((const CFStringRef) CFDictionaryGetValue(d->daDict,
             kDADiskDescriptionMediaNameKey));
    }
    return QString();
}
