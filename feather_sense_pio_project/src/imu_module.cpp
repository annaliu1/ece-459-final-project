#include "imu.h"
#include "sensor_manager.h"
#include <Wire.h>

// BNO08x reset pin / instance
#define BNO08X_RESET -1

sh2_SensorId_t reportType = SH2_ARVR_STABILIZED_RV; // desired report type
long reportIntervalUs = 5000;

sh2_SensorValue_t sensorValue;
Adafruit_BNO08x  bno08x(BNO08X_RESET);

//Function to set reportType | used in imu_sensor_init()
void setReports(sh2_SensorId_t reportType, long report_interval) {
  Serial.println("Setting desired reports");
  if (! bno08x.enableReport(reportType, report_interval)) {
    Serial.println("Could not enable stabilized remote vector");
  }
}

void quaternionToEuler(float qr, float qi, float qj, float qk, euler_t* ypr, bool degrees = false) {
  float sqr = sq(qr);
  float sqi = sq(qi);
  float sqj = sq(qj);
  float sqk = sq(qk);

  ypr->yaw = atan2(2.0 * (qi * qj + qk * qr), (sqi - sqj - sqk + sqr));
  ypr->pitch = asin(-2.0 * (qi * qk - qj * qr) / (sqi + sqj + sqk + sqr));
  ypr->roll = atan2(2.0 * (qj * qk + qi * qr), (-sqi - sqj + sqk + sqr));

  if (degrees) {
    ypr->yaw *= RAD_TO_DEG;
    ypr->pitch *= RAD_TO_DEG;
    ypr->roll *= RAD_TO_DEG;
  }
}

void quaternionToEulerRV(sh2_RotationVectorWAcc_t* rotational_vector, euler_t* ypr, bool degrees = false) {
  quaternionToEuler(rotational_vector->real, rotational_vector->i, rotational_vector->j, rotational_vector->k, ypr, degrees);
}

void imu_sensor_init(void) {
  // Bring up I2C quickly (no heavy probing here)
  Wire.begin();
  Wire.setClock(100000UL);

  // Try to acquire the shared I2C bus for a short time to probe the IMU.
  if (!sensor_bus_lock(pdMS_TO_TICKS(100))) {
    Serial.println("imu_sensor_init: failed to lock I2C for init - skipping probe");
    return;
  }

  // Do a single quick probe attempt using the library's I2C begin.
  bool found = false;
  if (bno08x.begin_I2C()) {
    found = true;
  }

  // Release the bus ASAP
  sensor_bus_unlock();

  if (!found) {
    Serial.println("imu_sensor_init: Failed to find BNO08x chip (probe attempt). Continuing without IMU.");
    return;
  }

  Serial.println("imu_sensor_init: BNO08x Found!");

  // Configure reports — wrap in bus lock because it likely uses I2C internally.
  if (sensor_bus_lock(pdMS_TO_TICKS(200))) {
    setReports(reportType, reportIntervalUs);
    sensor_bus_unlock();
  } else {
    Serial.println("imu_sensor_init: could not lock bus to call setReports(); try will occur later in reads.");
  }
}

bool readIMU(euler_t* ypr_in){
  if (bno08x.getSensorEvent(&sensorValue)) { // Checks if data is available
    quaternionToEulerRV(&sensorValue.un.arvrStabilizedRV, ypr_in, true);
    return true; // data was detected
  }

  return false; // data was not detected
}

