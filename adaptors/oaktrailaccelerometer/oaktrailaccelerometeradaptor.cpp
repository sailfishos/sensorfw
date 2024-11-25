#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

#include "config.h"
#include "oaktrailaccelerometeradaptor.h"
#include "logging.h"
#include "datatypes/utils.h"

OaktrailAccelAdaptor::OaktrailAccelAdaptor (const QString& id) :
    SysfsAdaptor (id, SysfsAdaptor::IntervalMode)
{
    struct stat st;

    devPath = SensorFrameworkConfig::configuration ()->value ("oaktrail_acc_sys_path").toString ();
    if ( lstat (devPath.toLatin1().constData(), &st) < 0 ) {
        qCWarning(lcSensorFw) << devPath << "no found";
        return;
    }

    devId = 0;
    addPath (devPath, devId);
    buffer = new DeviceAdaptorRingBuffer<OrientationData>(128);
    setAdaptedSensor("accelerometer", "Oaktrail accelerometer", buffer);

    setDescription("Oaktrail accelerometer");
    introduceAvailableDataRange(DataRange(-256, 256, 1));

    unsigned int min_interval_us =  10 * 1000;
    unsigned int max_interval_us = 586 * 1000;
    introduceAvailableInterval(DataRange(min_interval_us, max_interval_us, 0));

    unsigned int interval_us = 100 * 1000;
    setDefaultInterval(interval_us);
}

OaktrailAccelAdaptor::~OaktrailAccelAdaptor () {
    delete buffer;
}

bool OaktrailAccelAdaptor::startSensor () {
    if ( !(SysfsAdaptor::startSensor ()) )
        return false;

    qCInfo(lcSensorFw) << id() << "Oaktrail AccelAdaptor start";
    return true;
}

void OaktrailAccelAdaptor::stopSensor () {
    SysfsAdaptor::stopSensor();
    qCInfo(lcSensorFw) << id() << "Oaktrail AccelAdaptor stop";
}

void OaktrailAccelAdaptor::processSample (int pathId, int fd) {
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
