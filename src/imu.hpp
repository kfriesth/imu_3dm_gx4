/*
 * imu.hpp
 *
 *  Copyright (c) 2014 Kumar Robotics. All rights reserved.
 *
 *  This file is part of galt.
 *
 *  Created on: 2/6/2014
 *		  Author: gareth
 */

#ifndef IMU_H_
#define IMU_H_

#include <stdexcept>
#include <memory>
#include <string>
#include <deque>
#include <queue>
#include <vector>
#include <bitset>
#include <map>

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ //  will fail outside of gcc/clang
#define HOST_LITTLE_ENDIAN
#else
#define HOST_BIG_ENDIAN
#endif

namespace imu_3dm_gx4 {

/**
 * @brief Imu Interface to the Microstrain 3DM-GX4-25 IMU
 * @see http://www.microstrain.com/inertial/3dm-gx4-25
 * @author Gareth Cross
 *
 * @note Error handling: All methods which communicate with the device
 * can throw the exceptions below: io_error, timeout_error, command_error and
 * std::runtime_error. Additional exceptions are indicated on specific
 * functions.
 */
class Imu {
public:
  struct Packet {
    static constexpr uint8_t kHeaderLength = 4;
    static constexpr uint8_t kSyncMSB = 0x75;
    static constexpr uint8_t kSyncLSB = 0x65;

    union {
      struct {
        uint8_t syncMSB;
        uint8_t syncLSB;
      };
      uint16_t sync; /**< Identifer of packet */
    };

    uint8_t descriptor; /**< Type of packet */
    uint8_t length;     /**< Length of the packet in bytes */

    uint8_t payload[255]; /**< Payload of packet */

    union {
      struct {
        uint8_t checkMSB;
        uint8_t checkLSB;
      };
      uint16_t checksum; /**< Packet checksum */
    };

    /**
     * @brief True if this packet corresponds to an imu data message.
     */
    bool isIMUData() const;

    /**
     * @brief True if this packet corresponds to a filter data message
     */
    bool isFilterData() const;

    /**
     * @brief Extract the ACK code from this packet.
     * @param commmand Command packet to which this ACK should correspond.
     *
     * @return -1 if the packets do not correspond or this is not an ACK. The
     * error code is returned otherwise.
     */
    int ackErrorCodeFor(const Packet &command) const;

    /**
     * @brief Calculate the packet checksum. Sets the checksum variable
     */
    void calcChecksum();

    /**
     * @brief Constructor
     * @param desc Major descriptor of this command.
     * @param len Length of the packet payload.
     */
    Packet(uint8_t desc = 0);

    /**
     * @brief Make a 'human-readable' version of the packet.
     * @return std::string
     */
    std::string toString() const;
  } __attribute__((packed));

  /**
   * @brief Info Structure generated by the getDeviceInfo command
   */
  struct Info {
    uint16_t firmwareVersion;  /// Firmware version
    std::string modelName;     /// Model name
    std::string modelNumber;   /// Model number
    std::string serialNumber;  /// Serial number
    std::string lotNumber;     /// Lot number - appears to be unused
    std::string deviceOptions; /// Device options (range of the sensor)

    /**
     * @brief Conver to map of human readable strings.
     */
    std::map<std::string, std::string> toMap() const;
  };

  /**
   * @brief DiagnosticFields struct (See 3DM documentation for these fields)
   */
  struct DiagnosticFields {
    uint16_t modelNumber;
    uint8_t selector;
    uint32_t statusFlags;
    uint32_t systemTimer;
    uint32_t numPPSPulses;
    uint8_t imuStreamEnabled;
    uint8_t filterStreamEnabled;
    uint32_t imuPacketsDropped;
    uint32_t filterPacketsDropped;
    uint32_t comBytesWritten;
    uint32_t comBytesRead;
    uint32_t comNumWriteOverruns;
    uint32_t comNumReadOverruns;
    uint32_t usbBytesWritten;
    uint32_t usbBytesRead;
    uint32_t usbNumWriteOverruns;
    uint32_t usbNumReadOverruns;
    uint32_t numIMUParseErrors;
    uint32_t totalIMUMessages;
    uint32_t lastIMUMessage;
    uint16_t quatStatus;
    uint8_t beaconGood;
    uint8_t gpsTimeInit;

    /**
     * @brief Convert to map of human readable strings and integers.
     */
    std::map<std::string, unsigned int> toMap() const;

  } __attribute__((packed));

  /**
   * @brief IMUData IMU readings produced by the sensor
   */
  struct IMUData {
    enum {
      Accelerometer = (1 << 0),
      Gyroscope = (1 << 1),
      Magnetometer = (1 << 2),
      Barometer = (1 << 3),
      GpsTime = (1 << 4),
    };

    unsigned int fields; /**< Which fields are valid in the struct */

    float accel[3]; /**< Acceleration, units of G */
    float gyro[3];  /**< Angular rates, units of rad/s */
    float mag[3];   /**< Magnetic field, units of gauss */
    float pressure; /**< Pressure, units of gauss */
    double gpsTow;
    uint16_t gpsWeek;
    uint16_t gpsTimeStatus;

    IMUData() : fields(0) {}
  };

  /**
   * @brief FilterData Estimator readings produced by the sensor
   */
  struct FilterData {
    enum {
      Quaternion = (1 << 0),
      Bias = (1 << 1),
      AngleUnertainty = (1 << 2),
      BiasUncertainty = (1 << 3),
      GpsTime = (1 << 4),
    };

    unsigned int fields; /**< Which fields are present in the struct. */

    float quaternion[4]; /**< Orientation quaternion (q0,q1,q2,q3) */
    uint16_t quaternionStatus; /**< Quaternion status */

    float bias[3];       /**< Gyro bias */
    uint16_t biasStatus; /**< Bias status 0 = invalid, 1 = valid */

    float angleUncertainty[3];       /**< 1-sigma angle uncertainty */
    uint16_t angleUncertaintyStatus; /**< 0 = invalid, 1 = valid */

    float biasUncertainty[3];       /**< 1-sigma bias uncertainty */
    uint16_t biasUncertaintyStatus; /**< 0 = invalid, 1 = valid */

    double gpsTow;
    uint16_t gpsWeek;
    uint16_t gpsTimeStatus;

    FilterData() : fields(0) {}
  };

  /* Exceptions */

  /**
   * @brief command_error Generated when device replies with NACK.
   */
  struct command_error : public std::runtime_error {
    command_error(const Packet &p, uint8_t code);

  private:
    std::string generateString(const Packet &p, uint8_t code);
  };

  /**
   * @brief io_error Generated when a low-level IO command fails.
   */
  struct io_error : public std::runtime_error {
    io_error(const std::string &desc) : std::runtime_error(desc) {}
  };

  /**
   * @brief timeout_error Generated when read or write times out, usually
   * indicates device hang up.
   */
  struct timeout_error : public std::runtime_error {
    timeout_error(bool write, unsigned int to);

  private:
    std::string generateString(bool write, unsigned int to);
  };

  /**
   * @brief Imu Constructor
   * @param device Path to the device in /dev, eg. /dev/ttyACM0
   */
  Imu(const std::string &device);

  /**
   * @brief ~Imu Destructor
   * @note Calls disconnect() automatically.
   */
  virtual ~Imu();

  /**
   * @brief connect Open a file descriptor to the serial device.
   * @throw runtime_error if already open or path is invalid.
   * io_error for termios failures.
   */
  void connect();

  /**
   * @brief runOnce Poll for input and read packets if available.
   */
  void runOnce();

  /**
   * @brief disconnect Close the file descriptor, sending the IDLE command
   * first.
   */
  void disconnect();

  /**
   * @brief selectBaudRate Select baud rate.
   * @param baud The desired baud rate. Supported values are:
   * 9600,19200,115200,230400,460800,921600.
   *
   * @note This command will attempt to communicate w/ the device using all
   * possible baud rates. Once the current baud rate is determined, it will
   * switch to 'baud' and send the UART command.
   *
   * @throw std::runtime_error for invalid baud rates.
   */
  void selectBaudRate(unsigned int baud);

  /**
   * @brief ping Ping the device.
   * @param to Timeout in milliseconds.
   */
  void ping();

  /**
   * @brief idle Switch the device to idle mode.
   * @param to Timeout in milliseconds.
   */
  void idle();

  /**
   * @brief resume Resume the device.
   * @param to Timeout in milliseconds.
   */
  void resume();

  /**
   * @brief getDeviceInfo Get hardware information about the device.
   * @param info Struct into which information is placed.
   */
  void getDeviceInfo(Imu::Info &info);

  /**
   * @brief getIMUDataBaseRate Get the imu data base rate (should be 1000)
   * @param baseRate On success, the base rate in Hz
   */
  void getIMUDataBaseRate(uint16_t &baseRate);

  /**
   * @brief getFilterDataBaseRate Get the filter data base rate (should be 500)
   * @param baseRate On success, the base rate in Hz
   */
  void getFilterDataBaseRate(uint16_t &baseRate);

  /**
   * @brief getDiagnosticInfo Get diagnostic information from the IMU.
   * @param fields On success, a filled out DiagnosticFields.
   */
  void getDiagnosticInfo(Imu::DiagnosticFields &fields);

  /**
   * @brief setIMUDataRate Set imu data rate for different sources.
   * @param decimation Denominator in the update rate value: 1000/x
   * @param sources Sources to apply this rate to. May be a bitwise combination
   * of the values: Accelerometer, Gyroscope, Magnetometer, Barometer
   *
   * @throw invalid_argument if an invalid source is requested.
   */
  void setIMUDataRate(uint16_t decimation, const std::bitset<5> &sources);

  /**
   * @brief setFilterDataRate Set estimator data rate for different sources.
   * @param decimation Denominator in the update rate value: 500/x
   * @param sources Sources to apply this rate to. May be a bitwise combination
   * of the values: Quaternion, GyroBias, AngleUncertainty, BiasUncertainty.
   *
   * @throw invalid_argument if an invalid source is requested.
   */
  void setFilterDataRate(uint16_t decimation, const std::bitset<5> &sources);

  /**
   * @brief enableMeasurements Set which measurements to enable in the filter
   * @param accel If true, acceleration measurements are enabled
   * @param magnetometer If true, magnetometer measurements are enabled.
   */
  void enableMeasurements(bool accel, bool magnetometer);

  /**
   * @brief enableBiasEstimation Enable gyroscope bias estimation
   * @param enabled If true, bias estimation is enabled
   */
  void enableBiasEstimation(bool enabled);

  /**
   * @brief setHardIronOffset Set the hard-iron bias vector for the
   * magnetometer.
   * @param offset 3x1 vector, units of gauss.
   */
  void setHardIronOffset(float offset[3]);

  /**
   * @brief setSoftIronMatrix Set the soft-iron matrix for the magnetometer.
   * @param matrix 3x3 row-major matrix, default should be identity.
   */
  void setSoftIronMatrix(float matrix[9]);

  /**
   * @brief enableIMUStream Enable/disable streaming of IMU data
   * @param enabled If true, streaming is enabled.
   */
  void enableIMUStream(bool enabled);

  /**
   * @brief enableFilterStream Enable/disable streaming of estimation filter
   * data.
   * @param enabled If true, streaming is enabled.
   */
  void enableFilterStream(bool enabled);

  /**
     * @brief enableGpsTimeSync Enable/disable GPS time syncronization.
     * This requires system time syncronized to GPS time and PPS input to IMU
     * @param enabled If true, enable GPS time sync.
     */
  void enableGpsTimeSync(bool enabled);

  /**
   * @brief Set the IMU data callback.
   * @note The IMU data callback is called every time new IMU data is read.
   */
  void setIMUDataCallback(const std::function<void(const Imu::IMUData &)> &);

  /**
   * @brief Set the onboard filter data callback.
   * @note The filter data is called every time new orientation data is read.
   */
  void setFilterDataCallback(const std::function<void(const Imu::FilterData &)> &);

  /**
     * @brief Send a time update to the IMU.
     * @note This should be called once per seond with the current GPS time.
     * @param week Gps week
     * @param second Current Gps second of week
     */
  void sendGpsTimeUpdate(uint32_t week, uint32_t second);


private:
  //  non-copyable
  Imu(const Imu &) = delete;
  Imu &operator=(const Imu &) = delete;

  int pollInput(unsigned int to);

  int handleRead(size_t);

  void processPacket();

  int writePacket(const Packet &p, unsigned int to);

  void sendPacket(const Packet &p, unsigned int to);

  void receiveResponse(const Packet &command, unsigned int to);

  void sendCommand(const Packet &p);

  bool termiosBaudRate(unsigned int baud);

  const std::string device_;
  int fd_;
  unsigned int rwTimeout_;

  std::vector<uint8_t> buffer_;
  std::deque<uint8_t> queue_;
  size_t srcIndex_, dstIndex_;
  bool gpsSync_; /// Set when we want the timestamps synced to GPS time
  bool ppsBeaconGood, gpsTimeInitialized;
  uint32_t gpsTimeRefreshes, previousTimeRefresh;
  uint16_t quaternionStatus;
  std::function<void(const Imu::IMUData &)>
  imuDataCallback_; /// Called with IMU data is ready
  std::function<void(const Imu::FilterData &)>
  filterDataCallback_; /// Called when filter data is ready

  enum { Idle = 0, Reading, } state_;
  Packet packet_;
};

} //  imu_3dm_gx4

#endif // IMU_H_
