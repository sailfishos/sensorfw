#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <QFile>

#include "logging.h"
#include "config.h"
#include "oemtabletalsadaptor-ascii.h"
#include "datatypes/utils.h"
#include <stdlib.h>
#include <linux/types.h>
#include <unistd.h>

OEMTabletALSAdaptorAscii::OEMTabletALSAdaptorAscii(const QString& id) : SysfsAdaptor(id, SysfsAdaptor::IntervalMode)
{
    const unsigned int DEFAULT_RANGE = 65535;

    int range = DEFAULT_RANGE;
    QFile sysFile(SensorFrameworkConfig::configuration()->value("als-ascii_range_sysfs_path").toString());

    if (!(sysFile.open(QIODevice::ReadOnly))) {
        qCWarning(lcSensorFw) << id() << "Unable to config ALS range from sysfs, using default value: " << DEFAULT_RANGE;
    } else {
        sysFile.readLine(buf, sizeof(buf));
        range = QString(buf).toInt();
    }

    qCDebug(lcSensorFw) << id() << "Ambient light range: " << range;

    // Locate the actual handle
    QString devPath = SensorFrameworkConfig::configuration()->value("als-ascii_sysfs_path").toString();

    if (devPath.isEmpty())
    {
        qCWarning(lcSensorFw) << id() << "No driver handle found for ALS. Data not available.";
        return;
    }

    addPath(devPath);
    alsBuffer_ = new DeviceAdaptorRingBuffer<TimedUnsigned>(16);
    setAdaptedSensor("als", "Internal ambient light sensor lux values", alsBuffer_);

    setDescription("Ambient light");
    introduceAvailableDataRange(DataRange(0, range, 1));

    unsigned int min_interval_us = 10 * 1000;
    unsigned int max_interval_us = 98 * 1000;
    introduceAvailableInterval(DataRange(min_interval_us, max_interval_us, 0));

    unsigned int interval_us = 90 * 1000;
    setDefaultInterval(interval_us);
}

OEMTabletALSAdaptorAscii::~OEMTabletALSAdaptorAscii()
{
    delete alsBuffer_;
}

void OEMTabletALSAdaptorAscii::processSample(int pathId, int fd) {
    Q_UNUSED(pathId);

    if (read(fd, buf, sizeof(buf)) <= 0) {
        qCWarning(lcSensorFw) << id() << "read():" << strerror(errno);
        return;
    }
    buf[sizeof(buf)-1] = '\0';

    qCDebug(lcSensorFw) << id() << "Ambient light value: " << buf;

    __u16 idata = atoi(buf);

    TimedUnsigned* lux = alsBuffer_->nextSlot();

    lux->value_ = idata;
    lux->timestamp_ = Utils::getTimeStamp();

    alsBuffer_->commit();
    alsBuffer_->wakeUpReaders();
}
