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

#include "iokitstorageaccess.h"

#include <QDebug>

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>

using namespace Solid::Backends::IOKit;

class IOKitStorageAccess::Private
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

IOKitStorageAccess::IOKitStorageAccess(IOKitDevice *device)
    : DeviceInterface(device)
    , d(new Private(device))
{
    connect(device, SIGNAL(propertyChanged(QMap<QString,int>)),
            this, SLOT(onPropertyChanged(QMap<QString,int>)));
}

IOKitStorageAccess::IOKitStorageAccess(const IOKitDevice *device)
    : DeviceInterface(device)
    , d(new Private(device))
{
    connect(device, SIGNAL(propertyChanged(QMap<QString,int>)),
            this, SLOT(onPropertyChanged(QMap<QString,int>)));
}

IOKitStorageAccess::~IOKitStorageAccess()
{
    delete d;
}

bool IOKitStorageAccess::isAccessible() const
{
    filePath();
    const QVariant isMounted = m_device->property(QStringLiteral("isMounted"));
    return isMounted.isValid() && isMounted.toBool();
}

QString IOKitStorageAccess::filePath() const
{
    // mount points can change (but can they between invocations of filePath()?)
    if (d->daRef) {
        CFDictionaryRef dict = DADiskCopyDescription(d->daRef);
        if (dict) {
            const CFURLRef urlRef  = (const CFURLRef) CFDictionaryGetValue(dict, kDADiskDescriptionVolumePathKey);
            if (urlRef) {
                const CFStringRef mpRef = CFURLCopyFileSystemPath(urlRef, kCFURLPOSIXPathStyle);
                const QString mountPoint = QString::fromCFString(mpRef);
                const QString isMountedKey = QStringLiteral("isMounted");
                CFRelease(mpRef);
                CFRelease(dict);
                m_device->setProperty("mountPoint", QVariant(mountPoint));
                bool isMounted = !mountPoint.isEmpty();
                const QVariant wasMounted = m_device->property(isMountedKey);
                if (wasMounted.isValid() && wasMounted.toBool() != isMounted) {
                    IOKitStorageAccess(m_device).onPropertyChanged(QMap<QString,int>{{isMountedKey,isMounted}});
                }
                m_device->setProperty("isMounted", QVariant(isMounted));
                return mountPoint;
            }
        }
    }
    return QString();
}

bool IOKitStorageAccess::isIgnored() const
{
    if (m_device->iOKitPropertyExists(QStringLiteral("Open"))) {
        // ignore storage volumes that aren't mounted
        bool isIgnored = m_device->property(QStringLiteral("Open")).toBool() == false;
        m_device->setProperty("isIgnored", QVariant(isIgnored));
        return isIgnored;
    }
    const QVariant isIgnored = m_device->property(QStringLiteral("isIgnored"));
    return isIgnored.isValid() && isIgnored.toBool();
}

bool IOKitStorageAccess::setup()
{
    if (isAccessible()) {
        return false;
    } else {
        // TODO?
        return false;
    }
}

bool IOKitStorageAccess::teardown()
{
    if (!isAccessible()) {
        return false;
    } else {
        // TODO?
        return false;
    }
}

void IOKitStorageAccess::onPropertyChanged(const QMap<QString, int> &changes)
{
    Q_FOREACH (const QString &property, changes.keys()) {
        if (property == QStringLiteral("isMounted")) {
            emit accessibilityChanged(m_device->property(QStringLiteral("isMounted")).toBool(), m_device->udi());
        }
    }
}

