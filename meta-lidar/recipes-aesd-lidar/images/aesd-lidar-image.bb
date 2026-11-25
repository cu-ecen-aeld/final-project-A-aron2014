inherit core-image
CORE_IMAGE_EXTRA_INSTALL += "openssh"

inherit extrausers
# See https://docs.yoctoproject.org/singleindex.html#extrausers-bbclass
# We set a default password of root to match our busybox instance setup
# Don't do this in a production image
# PASSWD below is set to the output of
# printf "%q" $(mkpasswd -m sha256crypt root) to hash the "root" password
# string
PASSWD = "\$5\$2WoxjAdaC2\$l4aj6Is.EWkD72Vt.byhM5qRtF9HcCM/5YpbxpmvNB5"
EXTRA_USERS_PARAMS = "usermod -p '${PASSWD}' root;"
# Allow root user to login to the image
EXTRA_IMAGE_FEATURES:append = " allow-root-login"
# Networking: systemd-networkd only, with a static eth0 address from
# lidar-network. meta-ros adds connman by default; two managers on eth0 fight,
# and systemd-networkd-wait-online would never finish.
ROS_CONNECTION_MANAGER = ""
# The address itself is set in recipes-core/lidar-network/lidar-network.bb
CORE_IMAGE_EXTRA_INSTALL += "lidar-network"

# Extra free space on the root filesystem (in KB): +2 GB for logs, bags, tools
IMAGE_ROOTFS_EXTRA_SPACE = "2097152"

# Build an ext4 image for ease of use with runqemu
IMAGE_FSTYPES:append = " ext4"

#TODO: remove comment here to add to your image
CORE_IMAGE_EXTRA_INSTALL += " \
    ros-core \
    demo-nodes-cpp \
    kernel-module-cp210x \
    aesd-lidar \
"