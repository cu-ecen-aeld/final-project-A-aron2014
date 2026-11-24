SUMMARY = "ROS 2 driver for the LDROBOT D500 (STL-19P) lidar"
DESCRIPTION = "Serial reader thread, ring buffer, frame parser and LaserScan \
publisher (lidar_node), plus the ROS-free lidar_dump test tool."

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit ros_distro_lyrical
inherit ros_component

# ROS package name from package.xml. ros_component would otherwise derive
# "aesd_lidar" from the recipe name, and the FILES paths would not match.
ROS_BPN = "aesd_lidar_node"

SRC_URI = "git://github.com/cu-ecen-aeld/final-project-A-aron2014.git;protocol=https;branch=main"
PV = "0.1.0+git"
# Update to the final-project commit to build. It must include
# aesd-lidar-module/tools/ (CMakeLists.txt builds tools/lidar_dump.c).
SRCREV = "ee035d8aa602646ef93c3d8927199b35751e4377"

# The ROS package is a subdirectory of the final-project repo.
S = "${UNPACKDIR}/${BP}/aesd-lidar-module"

ROS_BUILD_DEPENDS = "rclcpp sensor-msgs"
ROS_BUILDTOOL_DEPENDS = "ament-cmake-native"
ROS_EXEC_DEPENDS = "rclcpp sensor-msgs launch launch-ros"

DEPENDS = "${ROS_BUILD_DEPENDS} ${ROS_BUILDTOOL_DEPENDS}"
RDEPENDS:${PN} += "${ROS_EXEC_DEPENDS}"

ROS_BUILD_TYPE = "ament_cmake"
inherit ros_${ROS_BUILD_TYPE}
