#
# This file was derived from the 'Hello World!' example recipe in the
# Yocto Project Development Manual.
#

SUMMARY = "mailbox Service"
SECTION = "mailbox"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://catch.c \
           file://catch.h \
           file://mailbox.service "

S = "${WORKDIR}"
TARGET_CC_ARCH += "${LDFLAGS}" 
#RDEPENDS_${PN} += "sdbusplus"
#RDEPENDS_${PN} += "libmapper"
RDEPENDS_${PN} += "libsystemd"
inherit obmc-phosphor-systemd

do_compile() {
	     ${CC} catch.c -o catch -lsystemd -lpthread -lmapper -lsdbusplus
}

do_install() {
	     install -d ${D}${bindir}
	     install -m 0755 catch ${D}${bindir}
#	     install -d ${D}${datadir}/catch
#             install -m 0755 config.json ${D}${datadir}/tempservice
             install -d ${D}${systemd_unitdir}/system
             install -m 0644 mailbox.service ${D}${systemd_unitdir}/system
}
FILES_${PN} += "${datadir}/*"
FILES_${PN} += "${systemd_unitdir}/*"

#LINK = "../mailbox.service:${SYSTEMD_DEFAULT_TARGET}.wants/mailbox.service"
#SYSTEMD_LINK_${PN} += "${LINK}"


SYSTEMD_LINK_${PN} += "../xyz.openbmc_project.RRC.service:${SYSTEMD_DEFAULT_TARGET}.wants/xyz.openbmc_project.RRC.service"


