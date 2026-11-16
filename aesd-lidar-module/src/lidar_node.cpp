// ROS 2 driver node for the LDROBOT D500 lidar.
//Data path: UART -> reader pthread -> ring buffer -> this node's worker
//thread -> ld_parser -> ld_scan -> LaserScan per scan.

#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"

#include "ld_lidar.h"
#include "serial_reader.h"

namespace
{
constexpr double kTwoPi = 2.0 * M_PI;
}

class LidarNode : public rclcpp::Node
{
public:
  LidarNode()
  : Node("lidar_node")
  {
    port_ = declare_parameter<std::string>("port", "/dev/ttyAMA0");
    frame_id_ = declare_parameter<std::string>("frame_id", "laser");
    const auto topic = declare_parameter<std::string>("topic", "scan");
    beams_ = declare_parameter<int>("beams", 500);
    range_min_ = declare_parameter<double>("range_min", 0.03);
    range_max_ = declare_parameter<double>("range_max", 12.0);

    if (beams_ < 1) {
      throw std::invalid_argument("parameter 'beams' must be at least 1");
    }

    publisher_ = create_publisher<sensor_msgs::msg::LaserScan>(
      topic, rclcpp::SensorDataQoS());

    ld_parser_init(&parser_);
    ld_scan_init(&assembler_);

    reader_ = serial_reader_open(port_.c_str());
    if (reader_ == nullptr) {
      throw std::runtime_error(
              "cannot open " + port_ + ": " + std::strerror(errno));
    }

    RCLCPP_INFO(
      get_logger(), "reading %s at %d baud, publishing %s (frame %s)",
      port_.c_str(), LD_DEFAULT_BAUD, publisher_->get_topic_name(),
      frame_id_.c_str());

    running_ = true;
    worker_ = std::thread(&LidarNode::run, this);
  }

  ~LidarNode() override
  {
    running_ = false;
    if (worker_.joinable()) {
      worker_.join();
    }
    serial_reader_close(reader_);
  }

private:
  void run()
  {
    uint8_t buf[512];

    while (running_ && rclcpp::ok()) {
      const ssize_t n = serial_reader_read(reader_, buf, sizeof(buf), 100);
      if (n < 0) {
        RCLCPP_FATAL(get_logger(), "serial port %s failed, shutting down", port_.c_str());
        rclcpp::shutdown();
        return;
      }
      if (n > 0) {
        ld_parser_feed(&parser_, buf, static_cast<size_t>(n), &LidarNode::frame_trampoline, this);
      }
    }
  }

  static void frame_trampoline(const ld_frame_t * frame, void * ctx)
  {
    static_cast<LidarNode *>(ctx)->on_frame(*frame);
  }

  void on_frame(const ld_frame_t & frame)
  {
    if (!ld_scan_add_frame(&assembler_, &frame, &completed_)) {
      return;
    }
    scans_++;
    dropped_points_ += completed_.dropped;
    if (completed_.complete) {
      publish_scan(completed_);
    }
  }

  void publish_scan(const ld_scan_t & scan)
  {
    const auto beams = static_cast<size_t>(beams_);
    const double increment = kTwoPi / static_cast<double>(beams);
    const double scan_time = scan.speed_dps > 0 ? 360.0 / scan.speed_dps : 0.1;

    sensor_msgs::msg::LaserScan msg;
    msg.header.stamp = now() - rclcpp::Duration::from_seconds(scan_time);
    msg.header.frame_id = frame_id_;
    msg.angle_min = 0.0f;
    msg.angle_max = static_cast<float>(kTwoPi - increment);
    msg.angle_increment = static_cast<float>(increment);
    msg.scan_time = static_cast<float>(scan_time);
    msg.time_increment = 0.0f;
    msg.range_min = static_cast<float>(range_min_);
    msg.range_max = static_cast<float>(range_max_);
    msg.ranges.assign(beams, std::numeric_limits<float>::quiet_NaN());
    msg.intensities.assign(beams, 0.0f);
    for (size_t i = 0; i < scan.count; i++) {
      const ld_scan_point_t & pt = scan.pts[i];
      if (pt.distance_mm == 0) {
        continue;
      }
      const double lidar_angle = pt.angle_cdeg * (M_PI / 18000.0);
      const double ros_angle = std::fmod(kTwoPi - lidar_angle, kTwoPi);
      const size_t index = static_cast<size_t>(std::lround(ros_angle / increment)) % beams;
      const float range = pt.distance_mm * 0.001f;
      if (std::isnan(msg.ranges[index]) || range < msg.ranges[index]) {
        msg.ranges[index] = range;
        msg.intensities[index] = static_cast<float>(pt.intensity);
      }
    }

    publisher_->publish(msg);
    log_stats();
  }

  void log_stats()
  {
    struct serial_reader_stats serial {};
    serial_reader_get_stats(reader_, &serial);

    RCLCPP_INFO_THROTTLE(
      get_logger(), *get_clock(), 60000,
      "scans=%u frames=%u crc_errors=%u bytes_discarded=%u dropped_points=%u overrun_bytes=%llu",
      scans_, parser_.stats.frames_ok, parser_.stats.crc_errors,
      parser_.stats.bytes_discarded, dropped_points_,
      static_cast<unsigned long long>(serial.overrun_bytes));
  }

  std::string port_;
  std::string frame_id_;
  int64_t beams_{500};
  double range_min_{0.03};
  double range_max_{12.0};

  rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr publisher_;
  struct serial_reader * reader_{nullptr};
  ld_parser_t parser_ {};
  ld_scan_assembler_t assembler_ {};
  ld_scan_t completed_ {};
  uint32_t scans_{0};
  uint32_t dropped_points_{0};
  std::atomic<bool> running_{false};
  std::thread worker_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  int rc = 0;
  try {
    rclcpp::spin(std::make_shared<LidarNode>());
  } catch (const std::exception & e) {
    RCLCPP_FATAL(rclcpp::get_logger("lidar_node"), "%s", e.what());
    rc = 1;
  }
  rclcpp::shutdown();
  return rc;
}
