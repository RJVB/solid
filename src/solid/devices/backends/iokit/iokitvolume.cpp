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

#include "iokitvolume.h"
#include "iokitgenericinterface.h"

#include <QDebug>

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>

using namespace Solid::Backends::IOKit;

class IOKitVolume::Private
{
public:
    Private(const IOKitDevice *device)
        : m_device(device)
    {
        daSession = DASessionCreate(kCFAllocatorDefault);
        if (daSession) {
            const QString devName = m_device->property(QStringLiteral("BSD Name")).toString();
            daRef = DADiskCreateFromBSDName(kCFAllocatorDefault, daSession, devName.toStdString().c_str());
        } else {
            daRef = 0;
        }
    }
    virtual ~Private()
    {
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
};

IOKitVolume::IOKitVolume(IOKitDevice *device)
    : Block(device)
    , d(new Private(device))
{
}

IOKitVolume::~IOKitVolume()
{
    delete d;
}

bool IOKitVolume::isIgnored() const
{
    // ignore storage volumes that aren't mounted
    return m_device->property(QStringLiteral("Open")).toBool() == false;
}

Solid::StorageVolume::UsageType IOKitVolume::usage() const
{
    const QString content = m_device->property(QStringLiteral("Content")).toString();

    if (content == QStringLiteral("CD_DA")) {
        // this is (probably) a CD track
        return Solid::StorageVolume::Other;
    }
    return Solid::StorageVolume::FileSystem;
}

QString IOKitVolume::fsType() const
{
    QString qFsType = QString();
    if (d->daRef) {
        CFDictionaryRef dict = DADiskCopyDescription(d->daRef);
        if (dict) {
            const CFStringRef fsType  = (const CFStringRef) CFDictionaryGetValue(dict, kDADiskDescriptionVolumeKindKey);
            qFsType = QString::fromCFString(fsType);
            CFRelease(dict);
        }
    }
    return qFsType;
}

QString IOKitVolume::label() const
{
    QString volLabel = QString();
    if (d->daRef) {
        CFDictionaryRef dict = DADiskCopyDescription(d->daRef);
        if (dict) {
            qWarning() << Q_FUNC_INFO;
            CFShow(dict);
            const CFStringRef volName = (const CFStringRef) CFDictionaryGetValue(dict, kDADiskDescriptionVolumeNameKey);
            volLabel = QString::fromCFString(volName);
            CFRelease(dict);
        }
    }
    return volLabel;
}

QString IOKitVolume::uuid() const
{
    return m_device->property(QStringLiteral("UUID")).toString();
}

qulonglong IOKitVolume::size() const
{
    return m_device->property(QStringLiteral("Size")).toULongLong();
}

QString Solid::Backends::IOKit::IOKitVolume::encryptedContainerUdi() const
{
    return QString();
}

