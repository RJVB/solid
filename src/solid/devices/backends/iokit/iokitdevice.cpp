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

#include "iokitdevice.h"
#include "iokitgenericinterface.h"
#include "iokitprocessor.h"
#include "iokitbattery.h"
#include "iokitstorage.h"
#include "iokitvolume.h"

#include <QtCore/qdebug.h>

#include <sys/types.h>
#include <sys/sysctl.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/network/IOEthernetInterface.h>

#include <CoreFoundation/CoreFoundation.h>

// from cfhelper.cpp
extern QMap<QString, QVariant> q_toVariantMap(const CFMutableDictionaryRef &dict);

namespace Solid
{
namespace Backends
{
namespace IOKit
{

// returns a solid type from an entry and its properties
static Solid::DeviceInterface::Type typeFromEntry(const io_registry_entry_t &entry,
        const QMap<QString, QVariant> &properties)
{
    if (IOObjectConformsTo(entry, "AppleACPICPU")) {
        return Solid::DeviceInterface::Processor;
    }
    if (IOObjectConformsTo(entry, "AppleSmartBattery")) {
        return Solid::DeviceInterface::Battery;
    }
    const QString bsdName = QStringLiteral("BSD Name"),
        leaf = QStringLiteral("Leaf");
    if (properties.contains(bsdName) && properties.value(bsdName).toString().startsWith(QStringLiteral("disk"))) {
        if (properties.contains(leaf) && properties.value(leaf).toBool() == false) {
            return Solid::DeviceInterface::StorageDrive;
        } else {
            return Solid::DeviceInterface::StorageVolume;
        }
    }
//     if (IOObjectConformsTo(entry, "IOCDMediaBSDClient")) {
//         return Solid::DeviceInterface::OpticalDrive;
//     }
//     qWarning() << "unsupported entry" << entry << "with properties" << properties;

    return Solid::DeviceInterface::Unknown;
}

// gets all properties from an entry into a QMap
static QMap<QString, QVariant> getProperties(const io_registry_entry_t &entry)
{
    CFMutableDictionaryRef propertyDict = 0;

    if (IORegistryEntryCreateCFProperties(entry, &propertyDict, kCFAllocatorDefault, kNilOptions) != KERN_SUCCESS) {
        return QMap<QString, QVariant>();
    }

    QMap<QString, QVariant> result = q_toVariantMap(propertyDict);

    CFRelease(propertyDict);

    return result;
}

// gets the parent's Udi from an entry
static QString getParentDeviceUdi(const io_registry_entry_t &entry)
{
    io_registry_entry_t parent = 0;
    kern_return_t ret = IORegistryEntryGetParentEntry(entry, kIOServicePlane, &parent);
    if (ret != KERN_SUCCESS) {
        // don't release parent here - docs say only on success
        return QString();
    }

    QString result;
    io_string_t pathName;
    ret = IORegistryEntryGetPath(parent, kIOServicePlane, pathName);
    if (ret == KERN_SUCCESS) {
        result = QString::fromUtf8(pathName);
    }

    // now we can release the parent
    IOObjectRelease(parent);

    return result;
}

static const QString computerModel()
{
    QString qModel = QString();
    char *model = NULL;
    size_t size = 0;
    if (sysctlbyname("hw.model", NULL, &size, NULL, 0) == 0 && size > 0) {
        model = new char [size];
        if (sysctlbyname("hw.model", model, &size, NULL, 0) == 0) {
            qModel= QLatin1String(model);
        }
        delete model;
    }
    return qModel;
}

class IOKitDevicePrivate
{
public:
    inline IOKitDevicePrivate()
        : type(Solid::DeviceInterface::Unknown)
        , parent(0)
    {}
    ~IOKitDevicePrivate()
    {
        if (parent) {
            delete parent;
            parent = 0;
        }
    }

    void init(const QString &udiString, const io_registry_entry_t &entry);

    QString udi;
    QString parentUdi;
    QMap<QString, QVariant> properties;
    Solid::DeviceInterface::Type type;
    IOKitDevice *parent;
};

void IOKitDevicePrivate::init(const QString &udiString, const io_registry_entry_t &entry)
{
    Q_ASSERT(entry != MACH_PORT_NULL);

    udi = udiString;

    properties = getProperties(entry);

    io_name_t className;
    IOObjectGetClass(entry, className);
    properties["className"] = QString::fromUtf8(className);

    parentUdi = getParentDeviceUdi(entry);
    type = typeFromEntry(entry, properties);
    if (type != Solid::DeviceInterface::Unknown) {
        qWarning() << "Solid: entry" << entry << "is type" << type << "with properties" << properties;
    }

    IOObjectRelease(entry);
}

IOKitDevice::IOKitDevice(const QString &udi, const io_registry_entry_t &entry)
    : d(new IOKitDevicePrivate)
{
    d->init(udi, entry);
}

IOKitDevice::IOKitDevice(const QString &udi)
    : d(new IOKitDevicePrivate)
{
    io_registry_entry_t entry = IORegistryEntryFromPath(
                                    kIOMasterPortDefault,
                                    udi.toLocal8Bit().constData());

    if (entry == MACH_PORT_NULL) {
        qDebug() << Q_FUNC_INFO << "Tried to create Device from invalid UDI" << udi;
        return;
    }

    d->init(udi, entry);
}

IOKitDevice::~IOKitDevice()
{
    delete d;
}

QString IOKitDevice::udi() const
{
    return d->udi;
}

QString IOKitDevice::parentUdi() const
{
    return d->parentUdi;
}

QString IOKitDevice::vendor() const
{
    QString vendor;
    if (parentUdi().isEmpty()) {
        return QStringLiteral("Apple");
    }
    switch (d->type) {
    case Solid::DeviceInterface::Processor:
        return Processor::vendor();
        break;
    case Solid::DeviceInterface::StorageDrive:
        return IOKitStorage(this).vendor();
        break;
    case Solid::DeviceInterface::StorageVolume:
        break;
    default:
        vendor = QString();
        break;
    }
    return vendor;
}

QString IOKitDevice::product() const
{
    if (parentUdi().isEmpty()) {
        return computerModel();
    }
    switch (d->type) {
    case Solid::DeviceInterface::Processor:
        return Processor::product();
        break;
    case Solid::DeviceInterface::StorageDrive:
        return IOKitStorage(this).product();
        break;
    case Solid::DeviceInterface::StorageVolume:
        break;
    }
    return QString(); // TODO
}

QString IOKitDevice::icon() const
{
    // adapted from HalDevice::icon()
    if (parentUdi().isEmpty()) {
        if (computerModel().contains(QStringLiteral("MacBook"))) {
            return "computer-laptop";
        } else {
            return "computer";
        }

    } else if (d->type == Solid::DeviceInterface::StorageDrive) {
        IOKitStorage drive(this);
        Solid::StorageDrive::DriveType driveType = drive.driveType();

        switch (driveType) {
        case Solid::StorageDrive::Floppy:
            return QStringLiteral("media-floppy");
            break;
        case Solid::StorageDrive::CdromDrive:
            return QStringLiteral("drive-optical");
            break;
        case Solid::StorageDrive::SdMmc:
            return QStringLiteral("media-flash-sd-mmc");
            break;
        case Solid::StorageDrive::CompactFlash:
            return QStringLiteral("media-flash-cf");
            break;
        }
        if (drive.bus() == Solid::StorageDrive::Usb) {
            return QStringLiteral("drive-removable-media-usb");
        }
        if (drive.isRemovable()) {
            return QStringLiteral("drive-removable-media");
        }
        return QStringLiteral("drive-harddisk");

    } else if (d->type == Solid::DeviceInterface::StorageVolume) {
    } else if (d->type == Solid::DeviceInterface::Battery) {
        return QStringLiteral("battery");
    } else if (d->type == Solid::DeviceInterface::Processor) {
        return QStringLiteral("cpu"); // FIXME: Doesn't follow icon spec
    } else {
        if (!d->parent) {
            d->parent = new IOKitDevice(parentUdi());
        }
        QString iconName = d->parent->icon();

        if (!iconName.isEmpty()) {
            return iconName;
        }

        return QStringLiteral("drive-harddisk");
    }
    return QString();
}

QStringList IOKitDevice::emblems() const
{
    return QStringList(); // TODO
}

QString IOKitDevice::description() const
{
    switch (d->type) {
    case Solid::DeviceInterface::StorageDrive:
        return IOKitStorage(this).description();
        break;
    }
    return product(); // TODO
}

QVariant IOKitDevice::property(const QString &key) const
{
    return d->properties.value(key);
}

QMap<QString, QVariant> IOKitDevice::allProperties() const
{
    return d->properties;
}

bool IOKitDevice::propertyExists(const QString &key) const
{
    return d->properties.contains(key);
}

bool IOKitDevice::queryDeviceInterface(const Solid::DeviceInterface::Type &type) const
{
    return (type == Solid::DeviceInterface::GenericInterface
            || type == d->type);
}

QObject *IOKitDevice::createDeviceInterface(const Solid::DeviceInterface::Type &type)
{
    QObject *iface = 0;

    switch (type) {
    case Solid::DeviceInterface::GenericInterface:
        iface = new GenericInterface(this);
        break;
    case Solid::DeviceInterface::Processor:
        if (d->type == Solid::DeviceInterface::Processor) {
            iface = new Processor(this);
        }
        break;
    case Solid::DeviceInterface::Battery:
        if (d->type == Solid::DeviceInterface::Battery) {
            iface = new Battery(this);
        }
        break;
    case Solid::DeviceInterface::StorageDrive:
        if (d->type == Solid::DeviceInterface::StorageDrive) {
            iface = new IOKitStorage(this);
        }
        break;
    case Solid::DeviceInterface::StorageVolume:
        if (d->type == Solid::DeviceInterface::StorageVolume) {
            iface = new IOKitVolume(this);
        }
        break;
        // the rest is TODO
    }

    return iface;
}

}
}
} // namespaces

