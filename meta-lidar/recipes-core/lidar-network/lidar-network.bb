SUMMARY = "Static IPv4 address for eth0 (systemd-networkd)"
DESCRIPTION = "Installs /etc/systemd/network/10-eth0.network. The 10- prefix \
sorts before systemd's default 80-wired.network (DHCP), so this file wins for eth0."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# Edit these here (an image recipe cannot override another recipe's
# variables; a global conf file such as local.conf or a fragment can).
# Address is CIDR (address/prefix). Leave GATEWAY or DNS empty to omit them,
# for example on a direct cable to the laptop.
LIDAR_ETH0_ADDRESS ?= "10.10.30.40/24"
LIDAR_ETH0_GATEWAY ?= ""
LIDAR_ETH0_DNS ?= ""

inherit allarch

do_configure[noexec] = "1"
do_compile[noexec] = "1"

do_install() {
    install -d ${D}${sysconfdir}/systemd/network
    f=${D}${sysconfdir}/systemd/network/10-eth0.network
    printf '[Match]\nName=eth0\n\n[Network]\nAddress=%s\n' "${LIDAR_ETH0_ADDRESS}" > $f
    if [ -n "${LIDAR_ETH0_GATEWAY}" ]; then
        printf 'Gateway=%s\n' "${LIDAR_ETH0_GATEWAY}" >> $f
    fi
    if [ -n "${LIDAR_ETH0_DNS}" ]; then
        printf 'DNS=%s\n' "${LIDAR_ETH0_DNS}" >> $f
    fi
    chmod 0644 $f
}

FILES:${PN} = "${sysconfdir}/systemd/network/10-eth0.network"
RDEPENDS:${PN} = "systemd-networkd"
