# OpenBMC userspace-based MCTP

Userspace-based MCTP stack is based on the [libmctp](https://github.com/openbmc/libmctp) program.

## Patches

All the necessary patches are currently in the review process, so you have to apply them locally.

OpenBMC patches:

`66498: libmctp: Drop 'astlpc-raw-kcs' PACKAGECONFIG option` | https://gerrit.openbmc.org/c/openbmc/openbmc/+/66498

`libmctp` patches (needed to be applied via a *.bbappend file):

`66497: astlpc: Provide KCS device path as a binding argument` | https://gerrit.openbmc.org/c/openbmc/libmctp/+/66497

`66467: Add MCTP over KCS transport binding` | https://gerrit.openbmc.org/c/openbmc/libmctp/+/66467

## Kconfig

Add the following kernel configuration option to the board Kconfig fragment:
```
CONFIG_IPMI_KCS_BMC_CDEV_RAW=y
```
This [driver](https://github.com/openbmc/linux/blob/aa3bee4aa1ff7988e101ed2f7be68b9e89bd3b5b/drivers/char/ipmi/kcs_bmc_cdev_raw.c#L291) would create `/dev/raw-kcsX` device for every `/dev/ipmi-kcsX` device in the system. The `/dev/raw-kcsX` devices expose to userspace the data and status registers of Keyboard-Controller-Style (KCS) IPMI interfaces via read() and write() syscalls ([documentation](https://github.com/openbmc/linux/blob/aa3bee4aa1ff7988e101ed2f7be68b9e89bd3b5b/Documentation/ABI/testing/dev-raw-kcs)).

Since the preferred approach for MCTP is via Linux kernel bindings, this driver is now considered obsolete. New designs should be based on the Linux kernel bindings, but for our test purposes we would be using it since it would be required for the `libmctp` binding.

## DTS

To the board DTS file we need to add a KCS channel with the enabled interrupt.

For example here is a configuration that I add to my DTS:
```
&kcs2 {
       status = "okay";
       aspeed,lpc-io-reg = <0xCA8 0xCA9>;
       aspeed,lpc-interrupts = <11 IRQ_TYPE_LEVEL_LOW>;
};
```
It is perfectly normal to have another KCS channel enabled in the system. For example on my board I also have kcs3 for the IPMI communication:
```
&kcs3 {
	status = "okay";
	aspeed,lpc-io-reg = <0xCA2>;
};
```

Keep in mind that kcs channels have some constraints about IO settings.

For example here are constraints for the IO port values (https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/char/ipmi/kcs_bmc_aspeed.c?h=v6.5#n210):
```
/*
 * We note D for Data, and C for Cmd/Status, default rules are
 *
 * 1. Only the D address is given:
 *   A. KCS1/KCS2 (D/C: X/X+4)
 *      D/C: CA0h/CA4h
 *      D/C: CA8h/CACh
 *   B. KCS3 (D/C: XX2/XX3h)
 *      D/C: CA2h/CA3h
 *   C. KCS4 (D/C: X/X+1)
 *      D/C: CA4h/CA5h
 *
 * 2. Both the D/C addresses are given:
 *   A. KCS1/KCS2/KCS4 (D/C: X/Y)
 *      D/C: CA0h/CA1h
 *      D/C: CA8h/CA9h
 *      D/C: CA4h/CA5h
 *   B. KCS3 (D/C: XX2/XX3h)
 *      D/C: CA2h/CA3h
 ```
 
There is also SIRQ constraint for the KCS1 channel that it can only use SIRQ1 or SIRQ12 (https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/char/ipmi/kcs_bmc_aspeed.c?h=v6.5#n172)

## Packages

Now add the following packages to your image:
```
libmctp
pldm
```

Also it is necessary to provide a recipe override for the `libmctp` kcs binding configuration (`recipes-phosphor/libmctp/libmctp_%.bbappend`):
```
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://mctp \
            "
do_install:append() {
        install -m 0644 ${WORKDIR}/mctp ${D}${sysconfdir}/default/mctp
}
```

The provided `recipes-phosphor/libmctp/libmctp/mctp` file contains target binding (in our case it is `kcs`) and a parameter for that binding (in our case it is KCS channel device, used for communication):
```
DEMUX_BINDING_OPTS=kcs /dev/raw-kcs2
```

## Building and flashing

After all of the above steps are applied you need to build OpenBMC image and flash it to the target device.

Now you are ready to test MCTP communication from the HOST via the [`PldmMessage` UEFI application](/#test-application).

## If communication is stuck

The `UEFI` and `libmctp` code are currently under development, so if you've encountered a situation with a broken KCS state machine, you can always reboot `libmctp` on the BMC side:
```
~# systemctl stop pldmd.service
~# systemctl stop mctp-demux.service
~# systemctl start mctp-demux.service
~# systemctl start pldmd.service
```
