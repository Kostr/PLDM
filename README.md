# Description

This repository contains a solution for the PLDM communication between the HOST (UEFI firmware) and BMC, with both implemented via open-source components:
- The HOST (UEFI firmware) part is based one the [`edk2`](https://github.com/tianocore/edk2) and [`edk2-platforms`](https://github.com/tianocore/edk2-platforms) code,
- The BMC part is based on the [openbmc](https://github.com/openbmc/openbmc) distribution.

The PLDM communication is provided via MCTP over KCS transport.

# HOST (UEFI firmware) configuration

The current `edk2-platfroms` repository has a [ManagebilityPkg](https://github.com/tianocore/edk2-platforms/tree/master/Features/ManageabilityPkg) that can be used to add support for the PLDM communication based on the MCTP over KCS transport.
The modules in the `ManagebilityPkg` can be used for different scenarios, so we have to configure them in the way that is needed for us:
- PLDM based on MCTP
- MCTP based on KCS

This can be done with the following configuration in the `DSC` file:
```
[Components]
  ManageabilityPkg/Universal/PldmProtocol/Dxe/PldmProtocolDxe.inf {
    <LibraryClasses>
      ManageabilityTransportLib|ManageabilityPkg/Library/ManageabilityTransportMctpLib/Dxe/DxeManageabilityTransportMctp.inf
  }

  ManageabilityPkg/Universal/MctpProtocol/Dxe/MctpProtocolDxe.inf {
    <LibraryClasses>
      ManageabilityTransportLib|ManageabilityPkg/Library/ManageabilityTransportKcsLib/Dxe/DxeManageabilityTransportKcs.inf
  }
```
For the successfull compilation we would also need to add a `ManageabilityTransportHelperLib` library:
```
[LibraryClasses]
  ManageabilityTransportHelperLib|ManageabilityPkg/Library/BaseManageabilityTransportHelperLib/BaseManageabilityTransportHelper.inf
```
And configure some PCDs:
```
[PcdsFixedAtBuild]
  gManageabilityPkgTokenSpaceGuid.PcdMctpSourceEndpointId|7
  gManageabilityPkgTokenSpaceGuid.PcdMctpDestinationEndpointId|8
  gManageabilityPkgTokenSpaceGuid.PcdMctpKcsBaseAddress|0xca8
```
Couple of words about PCDs:
- The `PcdMctpSourceEndpointId` can be arbitrary,
- The `PcdMctpDestinationEndpointId` is mapped to the default `libmctp` EID (https://github.com/openbmc/libmctp/blob/2a2a0f6fd83318cfc37f44a657c9490c6a58a03a/utils/mctp-demux-daemon.c#L43),
- The `PcdMctpKcsBaseAddress` should be chosen based on your hardware settings and synced with the BMC settings. In this case I use `0xca8`, later this value would be seen in the BMC configuration.


Also the current code is not fully complaint with the [Management Component Transport Protocol (MCTP) KCS Transport Binding Specification](https://www.dmtf.org/sites/default/files/standards/documents/DSP0254_1.0.0.pdf).
As a temprarily workaround we would be using the following [fork](https://github.com/Kostr/edk2-platforms).

# BMC configuration

The MCTP communication in OpenBMC can be based on the
- userspace application ([libmctp](https://github.com/openbmc/libmctp) approach)
- kernel net mctp driver ([info](https://codeconstruct.com.au/docs/mctp-on-linux-introduction/), [current bindings](https://github.com/openbmc/linux/tree/dev-6.1/drivers/net/mctp))

Both approaches currently lack support for the MCTP over KCS binding.

In the current solution we would be adding MCTP over KCS support to the libmctp (1st approach) since it is easier to implement.

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

iThe provided `recipes-phosphor/libmctp/libmctp/mctp` file contains target binding (in our case it is `kcs`) and a parameter for that binding (in our case it is KCS channel device, used for communication):
```
DEMUX_BINDING_OPTS=kcs /dev/raw-kcs2
```

## Building and flashing

After all of the above steps are applied you need to build OpenBMC image and flash it to the target device.

## If communication is stuck

The `UEFI`/`libmctp` code currently is under development, so if you've encountered a situation with a broken state machine, you can always reboot `libmctp` on the BMC side:
```
~# systemctl stop pldmd.service
~# systemctl stop mctp-demux.service
~# systemctl start mctp-demux.service
~# systemctl start pldmd.service
```

# Test application

The repository contains a simple application for the UEFI shell ([`PldmMessage`](PldmMessage)), that utilizes `EDKII_MCTP_PROTOCOL` to send `GetPLDMTypes (0x04)` command

BMC handling for this command is located [here](https://github.com/openbmc/pldm/blob/19fdb45ba28835686b70f27c9819ff34145a1794/libpldmresponder/base.cpp#L74).

# Putting everything together

Now when everything is in place, launch `PldmMessage.efi` from the UEFI shell. Here is the expected output:
```
FS0:\> PldmMessage_GetTypes.efi
MCTP protocol version 256
PldmRequestBuffer[0]=0x95
PldmRequestBuffer[1]=0x00
PldmRequestBuffer[2]=0x04
PldmResponse[0]=0x1D
PldmResponse[1]=0x00
PldmResponse[2]=0x00
PldmResponse[3]=0x00
PldmResponse[4]=0x00
PldmResponse[5]=0x00
PldmResponse[6]=0x00
PldmResponse[7]=0x00
```

# Next steps

After pushing the current patches to the `libmctp`/`openbmc` and `edk2-platforms` repos, the next step would be to write kernel net driver for the MCTP over KCS binding ([info](https://codeconstruct.com.au/docs/mctp-on-linux-introduction/), [current bindings](https://github.com/openbmc/linux/tree/dev-6.1/drivers/net/mctp))
