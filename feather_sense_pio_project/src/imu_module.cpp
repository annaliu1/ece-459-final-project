#include "imu.h"

#define BNO08X_RESET -1

sh2_SensorId_t reportType = SH2_ARVR_STABILIZED_RV; //Slower frequency (at most 250Hz) but more accurate
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

void imu_sensor_init(void){
  // Try to initialize!
  if (!bno08x.begin_I2C()) {
      Serial.println("Failed to find BNO08x chip");
      // while (1) { delay(10); } -> Would delay for the entire program if not found
    }
    Serial.println("BNO08x Found!");
  
  setReports(reportType, reportIntervalUs);
}

bool readIMU(euler_t* ypr_in){
  if (bno08x.getSensorEvent(&sensorValue)) { //Checks if data is available
    quaternionToEulerRV(&sensorValue.un.arvrStabilizedRV, ypr_in, true);
    return true; //data was detected
  }

  return false; //data was not detected
}