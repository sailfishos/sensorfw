#include "oaktrailaccelerometeradaptorplugin.h"
#include "oaktrailaccelerometeradaptor.h"
#include "sensormanager.h"
#include "logging.h"

void OaktrailAccelerometerAdaptorPlugin::Register(class Loader&)
{
    qCInfo(lcSensorFw) << id() << "registering oaktrailaccelerometeradaptor";
    SensorManager& sm = SensorManager::instance();
    sm.registerDeviceAdaptor<OaktrailAccelAdaptor>("accelerometeradaptor");
}
