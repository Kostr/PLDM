# OpenBMC kernel-based MCTP

Kernel-based MCTP stack is based on the kernel net mctp driver ([info](https://codeconstruct.com.au/docs/mctp-on-linux-introduction/), [current bindings](https://github.com/openbmc/linux/tree/dev-6.1/drivers/net/mctp))
.

## Configuration

For the kernel-based MCTP stack it is necessary to enable `mctp` disto feature.

Add the following string to your machine configuration file:
```
DISTRO_FEATURES += "mctp"
```

## Patches

Since currently there is no MCTP-over-KCS binding driver we need to add some patches to the kernel:

- [0001-ipmi-Move-KCS-headers-to-common-include-folder.patch](0001-ipmi-Move-KCS-headers-to-common-include-folder.patch)
- [0002-ipmi-Create-header-with-KCS-interface-defines.patch](0002-ipmi-Create-header-with-KCS-interface-defines.patch)
- [0003-mctp-Add-MCTP-over-KCS-transport-binding.patch](0003-mctp-Add-MCTP-over-KCS-transport-binding.patch)

You also need to apply this patch to the [libpldm](https://github.com/openbmc/libpldm) library:
- [`66591: transport: af-mctp: Add pldm_transport_af_mctp_bind()`](https://gerrit.openbmc.org/c/openbmc/libpldm/+/66591)

And this patch to the [pldm](https://github.com/openbmc/pldm) package:
- [`63652: pldm: Convert to using libpldm transport APIs`](https://gerrit.openbmc.org/c/openbmc/pldm/+/63652)

Add all of these patches to your OpenBMC distibution via a standard recipe override mechanics.

## Kconfig

Add the following kernel configuration options to the board Kconfig fragment:
```
CONFIG_MCTP=y
CONFIG_MCTP_TRANSPORT_KCS=y
```

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

Add the following package to your image:
```
pldm
```

## Configuring `mctpkcs2` interface

Since mctp is a network device, before performing actual communication you have to open the interface and setup address and routing. Offcourse you can always do this manually by executing these commands on the running BMC system:
```
$ mctp addr add 8 dev mctpkcs2
$ mctp route add 9 via mctpkcs2
$ ifconfig mctpkcs2 up
```
But it would be better to write these commands in a systemd service that would be executed on every system start. You can find example of a recipe that adds such unit to the system in the [recipes-support](recipes-support) folder.

## Building and flashing

After all of the above steps are applied you need to build OpenBMC image and flash it to the target device.

Now you are ready to test MCTP communication from the HOST via the [`PldmMessage` UEFI application](/#test-application).

## If communication is stuck

The `UEFI` and `mctpkcs` code are currently under development, so if you've encountered a situation with a broken KCS state machine, you can always reboot MCTP network interface on the BMC side:
```
$ ifconfig mctpkcs2 down
$ ifconfig mctpkcs2 up
```
