#ifndef PEGATRONACCELEROMETERADAPTOR_H
#define PEGATRONACCELEROMETERADAPTOR_H

#include "inputdevadaptor.h"
#include "deviceadaptorringbuffer.h"
#include "datatypes/orientationdata.h"
#include <QTime>

class PegatronAccelerometerAdaptor : public InputDevAdaptor
{
    Q_OBJECT;
public:
    /**
     * Factory method for gaining a new instance of AccelerometerAdaptor class.
     * @param id Identifier for the adaptor.
     */
    static DeviceAdaptor* factoryMethod(const QString& id)
    {
        return new PegatronAccelerometerAdaptor(id);
    }

protected:
    /**
     * Constructor.
     * @param id Identifier for the adaptor.
     */
    PegatronAccelerometerAdaptor(const QString& id);
    ~PegatronAccelerometerAdaptor();

private:
    DeviceAdaptorRingBuffer<OrientationData>* accelerometerBuffer_;
    OrientationData orientationValue_;
    QTime time;

    void interpretEvent(int src, struct input_event *ev);
    void commitOutput(struct input_event *ev);
    void interpretSync(int src, struct input_event *ev);
};

#endif
