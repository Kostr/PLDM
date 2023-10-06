FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI:append = " file://mctpkcs2.service"

SYSTEMD_SERVICE:${PN} += "mctpkcs2.service"

do_install:append () {
    if ${@bb.utils.contains('PACKAGECONFIG', 'systemd', 'true', 'false', d)}; then
        install -d ${D}${systemd_system_unitdir}
        install -m 0644 ${WORKDIR}/mctpkcs2.service ${D}${systemd_system_unitdir}
    fi
}
