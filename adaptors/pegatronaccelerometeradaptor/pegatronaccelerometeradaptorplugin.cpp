#include "pegatronaccelerometeradaptorplugin.h"
#include "pegatronaccelerometeradaptor.h"
#include "sensormanager.h"
#include "logging.h"

void PegatronAccelerometerAdaptorPlugin::Register(class Loader&)
{
    qCInfo(lcSensorFw) << id() << "registering pegatronaccelerometeradaptor";
    SensorManager& sm = SensorManager::instance();
    sm.registerDeviceAdaptor<PegatronAccelerometerAdaptor>("accelerometeradaptor");
}
