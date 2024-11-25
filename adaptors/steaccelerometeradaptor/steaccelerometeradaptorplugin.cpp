#include "steaccelerometeradaptorplugin.h"
#include "steaccelerometeradaptor.h"
#include "sensormanager.h"
#include "logging.h"

void SteAccelerometerAdaptorPlugin::Register(class Loader&)
{
    qCInfo(lcSensorFw) << id() << "registering steaccelerometeradaptor";
    SensorManager& sm = SensorManager::instance();
    sm.registerDeviceAdaptor<SteAccelAdaptor>("accelerometeradaptor");
}
