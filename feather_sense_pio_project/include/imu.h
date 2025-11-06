#include <Arduino.h>
#include <Adafruit_BNO08x.h>
#include "sensor_manager.h"

struct euler_t {
    float yaw;
    float pitch;
    float roll;
};

extern euler_t ypr; //Extern (now only define it in ONE .cpp file)