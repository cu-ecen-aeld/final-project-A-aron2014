# Build ROS 2 without the Zenoh RMW (avoids meta-zenoh and a Rust toolchain).
# Fast DDS and Cyclone DDS are still built and installed.
ROS_BUILD_DEPENDS:remove = "rmw-zenoh-cpp"
