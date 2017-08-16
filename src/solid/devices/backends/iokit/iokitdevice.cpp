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
#include "iokitstorageaccess.h"
#include "iokitvolume.h"
#include "iokitopticaldrive.h"

#include <QDebug>
#include <QSet>

#include <sys/types.h>
#include <sys/sysctl.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/network/IOEthernetInterface.h>

#include <CoreFoundation/CoreFoundation.h>

// from cfhelper.cpp
extern QMap<QString, QVariant> q_toVariantMap(const CFMutableDictionaryRef &dict);

typedef QSet<Solid::DeviceInterface::Type> DeviceInterfaceTypes;

namespace Solid
{
namespace Backends
{
namespace IOKit
{

// returns a solid type from an entry and its properties
static DeviceInterfaceTypes typesFromEntry(const io_registry_entry_t &entry,
        const QMap<QString, QVariant> &properties,
        Solid::DeviceInterface::Type &mainType)
{
    DeviceInterfaceTypes types;
    mainType = Solid::DeviceInterface::Unknown;
    if (IOObjectConformsTo(entry, "AppleACPICPU")) {
        mainType = Solid::DeviceInterface::Processor;
        types << mainType;
    }
    if (IOObjectConformsTo(entry, "AppleSmartBattery")) {
        mainType = Solid::DeviceInterface::Battery;
        types << mainType;
    }
    const QString bsdName = QStringLiteral("BSD Name"),
        leaf = QStringLiteral("Leaf");
    if (IOObjectConformsTo(entry, "IOCDMedia")
        || IOObjectConformsTo(entry, "IODVDMedia")
        || IOObjectConformsTo(entry, "IOCDMedia")) {
        mainType = Solid::DeviceInterface::OpticalDrive;
        types << mainType;
    }
    if (properties.contains(bsdName) && properties.value(bsdName).toString().startsWith(QStringLiteral("disk"))) {
        if (properties.contains(leaf) && properties.value(leaf).toBool() == false) {
            if (mainType == Solid::DeviceInterface::Unknown) {
                mainType = Solid::DeviceInterface::StorageDrive;
            }
            types << Solid::DeviceInterface::StorageDrive;
        } else if (mainType == Solid::DeviceInterface::Unknown) {
            mainType = Solid::DeviceInterface::StorageVolume;
        }
        types << Solid::DeviceInterface::StorageVolume;
    }

    if (types.isEmpty()) {
        types << mainType;
//         qWarning() << "unsupported entry" << entry << "with properties" << properties;
    }

    return types;
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

    io_name_t className;
    IOObjectGetClass(entry, className);
    result["className"] = QString::fromUtf8(className);

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
        : parent(0)
    {
        type << Solid::DeviceInterface::Unknown;
    }
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
    DeviceInterfaceTypes type;
    Solid::DeviceInterface::Type mainType;
    IOKitDevice *parent;
};

void IOKitDevicePrivate::init(const QString &udiString, const io_registry_entry_t &entry)
{
    Q_ASSERT(entry != MACH_PORT_NULL);

    udi = udiString;

    properties = getProperties(entry);

    parentUdi = getParentDeviceUdi(entry);
    type = typesFromEntry(entry, properties, mainType);
    if (mainType != Solid::DeviceInterface::Unknown) {
//         qWarning() << "Solid: entry" << entry << "mainType=" << mainType << "typeList:" << type
//             << "with properties" << properties;
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
    if (udi.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "Tried to create Device from empty UDI";
        return;
    }

    io_registry_entry_t entry = IORegistryEntryFromPath(
                                    kIOMasterPortDefault,
                                    udi.toLocal8Bit().constData());

    if (entry == MACH_PORT_NULL) {
        qWarning() << Q_FUNC_INFO << "Tried to create Device from invalid UDI" << udi;
        return;
    }

    d->init(udi, entry);
}

IOKitDevice::IOKitDevice(const IOKitDevice &device)
    : d(new IOKitDevicePrivate)
{
    if (device.udi().isEmpty()) {
        qWarning() << Q_FUNC_INFO << "Tried to create Device from empty UDI";
        return;
    }

    io_registry_entry_t entry = IORegistryEntryFromPath(
                                    kIOMasterPortDefault,
                                    device.udi().toLocal8Bit().constData());

    if (entry == MACH_PORT_NULL) {
        qWarning() << Q_FUNC_INFO << "Tried to create Device from invalid UDI" << device.udi();
        return;
    }

    d->init(device.udi(), entry);
}

IOKitDevice::~IOKitDevice()
{
    delete d;
}

bool IOKitDevice::conformsToIOKitClass(const QString &className) const
{
    bool conforms = false;
    if (!className.isEmpty()) {
        io_registry_entry_t entry = IORegistryEntryFromPath(
                                    kIOMasterPortDefault,
                                    udi().toLocal8Bit().constData());
        if (entry != MACH_PORT_NULL) {
            conforms = IOObjectConformsTo(entry, className.toLocal8Bit().constData());
            IOObjectRelease(entry);
        }
    }
    return conforms;
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
    switch (d->mainType) {
    case Solid::DeviceInterface::Processor:
        return Processor::vendor();
        break;
    case Solid::DeviceInterface::Battery:
        return property(QStringLiteral("Manufacturer")).toString();
        break;
    case Solid::DeviceInterface::StorageDrive:
    case Solid::DeviceInterface::OpticalDrive:
        return IOKitStorage(this).vendor();
        break;
    case Solid::DeviceInterface::StorageVolume:
        return IOKitVolume(this).vendor();
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
//     if (d->mainType != Solid::DeviceInterface::Unknown) {
//         qWarning() << Q_FUNC_INFO << "mainType=" << d->mainType;
//     }
    switch (d->mainType) {
    case Solid::DeviceInterface::Processor:
        return Processor::product();
        break;
    case Solid::DeviceInterface::Battery:
        return property(QStringLiteral("DeviceName")).toString();
        break;
    case Solid::DeviceInterface::StorageDrive:
    case Solid::DeviceInterface::OpticalDrive:
        return IOKitStorage(this).product();
        break;
    case Solid::DeviceInterface::StorageVolume:
        return IOKitVolume(this).product();
        break;
    }
    return QString(); // TODO
}

QString IOKitDevice::description() const
{
    switch (d->mainType) {
    case Solid::DeviceInterface::Processor:
        return QStringLiteral("Processor");
        break;
    case Solid::DeviceInterface::Battery:
        return QStringLiteral("Apple Smart Battery");
        break;
    case Solid::DeviceInterface::StorageDrive:
    case Solid::DeviceInterface::OpticalDrive:
        return IOKitStorage(this).description();
        break;
    case Solid::DeviceInterface::StorageVolume:
        return IOKitVolume(this).description();
        break;
     }
    return product(); // TODO
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

    } else if (d->mainType == Solid::DeviceInterface::StorageDrive) {
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

    } else if (d->mainType == Solid::DeviceInterface::StorageVolume) {
    } else if (d->mainType == Solid::DeviceInterface::Battery) {
        return QStringLiteral("battery");
    } else if (d->mainType == Solid::DeviceInterface::Processor) {
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

QVariant IOKitDevice::property(const QString &key) const
{
    if (!d->properties.contains(key)) {
        return QObject::property(key.toUtf8());
    }
    return d->properties.value(key);
}

QMap<QString, QVariant> IOKitDevice::allProperties() const
{
    return d->properties;
}

bool IOKitDevice::iOKitPropertyExists(const QString &key) const
{
    return d->properties.contains(key);
}

bool IOKitDevice::queryDeviceInterface(const Solid::DeviceInterface::Type &type) const
{
    bool ret = false;
    switch (type) {
    case Solid::DeviceInterface::GenericInterface:
        ret = true;
        break;
    case Solid::DeviceInterface::StorageAccess:
        if (d->type.contains(Solid::DeviceInterface::StorageDrive)
            || d->type.contains(Solid::DeviceInterface::StorageVolume)) {
            ret = true;
        }
        break;
    default:
        ret = d->type.contains(type);
        break;
    }
    return ret;
}

QObject *IOKitDevice::createDeviceInterface(const Solid::DeviceInterface::Type &type)
{
    QObject *iface = 0;

    switch (type) {
    case Solid::DeviceInterface::GenericInterface:
        iface = new GenericInterface(this);
        break;
    case Solid::DeviceInterface::Processor:
        if (d->type.contains(Solid::DeviceInterface::Processor)) {
            iface = new Processor(this);
        }
        break;
    case Solid::DeviceInterface::Battery:
        if (d->type.contains(Solid::DeviceInterface::Battery)) {
            iface = new Battery(this);
        }
        break;
    case Solid::DeviceInterface::OpticalDrive:
        if (d->type.contains(Solid::DeviceInterface::OpticalDrive)) {
            iface = new IOKitOpticalDrive(this);
        }
        break;
    case Solid::DeviceInterface::StorageDrive:
        if (d->type.contains(Solid::DeviceInterface::StorageDrive)) {
            iface = new IOKitStorage(this);
        }
        break;
    case Solid::DeviceInterface::Block:
        if (d->type.contains(Solid::DeviceInterface::StorageDrive)
            || d->type.contains(Solid::DeviceInterface::StorageVolume)) {
            iface = new Block(this);
        }
        break;
    case Solid::DeviceInterface::StorageVolume:
        if (d->type.contains(Solid::DeviceInterface::StorageVolume)) {
            iface = new IOKitVolume(this);
        }
        break;
    case Solid::DeviceInterface::StorageAccess:
        if (d->type.contains(Solid::DeviceInterface::StorageDrive)
            || d->type.contains(Solid::DeviceInterface::StorageVolume)) {
            iface = new IOKitStorageAccess(this);
        }
        break;
        // the rest is TODO
    }

    return iface;
}

}
}
} // namespaces

