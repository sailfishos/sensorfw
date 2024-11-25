#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

#include "config.h"
#include "oemtabletaccelerometeradaptor.h"
#include "logging.h"
#include "datatypes/utils.h"

OemtabletAccelAdaptor::OemtabletAccelAdaptor (const QString& id) :
    SysfsAdaptor (id, SysfsAdaptor::IntervalMode)
{
    struct stat st;

    devPath = SensorFrameworkConfig::configuration ()->value ("oem_tablet_acc_sys_path").toString ();
    if ( lstat (devPath.toLatin1().constData(), &st) < 0 ) {
        qCWarning(lcSensorFw) << devPath << "no found";
        return;
    }

    devId = 0;
    addPath (devPath, devId);
    buffer = new DeviceAdaptorRingBuffer<OrientationData>(128);
    setAdaptedSensor("accelerometer", "OEM tablet accelerometer", buffer);

    setDescription("OEM tablet accelerometer");
    introduceAvailableDataRange(DataRange(-256, 256, 1));

    unsigned int min_interval_us =  10 * 1000;
    unsigned int max_interval_us = 586 * 1000;
    introduceAvailableInterval(DataRange(min_interval_us, max_interval_us, 0));

    // FIXME: what meaning zero default interval is supposed to have here?
    // setDefaultInterval(0);
}

OemtabletAccelAdaptor::~OemtabletAccelAdaptor () {
    delete buffer;
}

bool OemtabletAccelAdaptor::startSensor () {
    if ( !(SysfsAdaptor::startSensor ()) )
        return false;

    qCInfo(lcSensorFw) << id() << "OEM tablet AccelAdaptor start";
    return true;
}

void OemtabletAccelAdaptor::stopSensor () {
    SysfsAdaptor::stopSensor();
    qCInfo(lcSensorFw) << id() << "OEM tablet AccelAdaptor stop";
}

void OemtabletAccelAdaptor::processSample (int pathId, int fd) {
    char buf[32];
    int x, y, z;

    if ( pathId != devId ) {
        qCWarning(lcSensorFw) << "Wrong pathId" << pathId;
        return;
    }

    lseek (fd, 0, SEEK_SET);
    if ( read (fd, buf, sizeof(buf)) < 0 ) {
        qCWarning(lcSensorFw) << "Read failed";
        return;
    }

    if ( sscanf (buf, "(%d,%d,%d)", &x, &y, &z) == 0 ) {
        qCWarning(lcSensorFw) << "Wrong data format";
        return;
    }

    OrientationData* d = buffer->nextSlot ();
    d->timestamp_ = Utils::getTimeStamp();
    d->x_ = x;
    d->y_ = y;
    d->z_ = z;

    buffer->commit ();
    buffer->wakeUpReaders ();
}
