#include <errno.h>

#include "logging.h"
#include "config.h"
#include "datatypes/utils.h"

#include "oemtabletgyroscopeadaptor.h"

OEMTabletGyroscopeAdaptor::OEMTabletGyroscopeAdaptor(const QString& id) :
    SysfsAdaptor(id, SysfsAdaptor::IntervalMode)
{
    gyroscopeBuffer_ = new DeviceAdaptorRingBuffer<TimedXyzData>(32);

    setAdaptedSensor("gyroscope", "mpu3050", gyroscopeBuffer_);

    introduceAvailableDataRange(DataRange(-32768, 32767, 1));

    unsigned int min_interval_us =  10 * 1000;
    unsigned int max_interval_us = 113 * 1000;
    introduceAvailableInterval(DataRange(min_interval_us, max_interval_us, 0));

    unsigned int interval_us = 100 * 1000;
    setDefaultInterval(interval_us);
}

OEMTabletGyroscopeAdaptor::~OEMTabletGyroscopeAdaptor()
{
    delete gyroscopeBuffer_;
}

void OEMTabletGyroscopeAdaptor::processSample(int pathId, int fd)
{
    Q_UNUSED(pathId);
    short x, y, z;
    char buf[32];

    if (read(fd, buf, sizeof(buf)) <= 0) {
        qCWarning(lcSensorFw) << id() << "read():" << strerror(errno);
        return;
    }
    qCDebug(lcSensorFw) << id() << "gyroscope output value: " << buf;

    sscanf(buf, "%hd %hd %hd\n", &x, &y, &z);

    TimedXyzData* pos = gyroscopeBuffer_->nextSlot();
    pos->x_ = x;
    pos->y_ = y;
    pos->z_ = z;
    pos->timestamp_ = Utils::getTimeStamp();

    gyroscopeBuffer_->commit();
    gyroscopeBuffer_->wakeUpReaders();
}
