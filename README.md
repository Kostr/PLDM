# Description

This repository contains a solution for the [PLDM](https://www.dmtf.org/sites/default/files/standards/documents/DSP0240_1.1.0.pdf) communication between the HOST (UEFI firmware) and BMC, with both implemented via open-source components:
- The HOST (UEFI firmware) part is based one the [`edk2`](https://github.com/tianocore/edk2) and [`edk2-platforms`](https://github.com/tianocore/edk2-platforms) code,
- The BMC part is based on the [openbmc](https://github.com/openbmc/openbmc) distribution.

The PLDM communication is provided via [MCTP over KCS transport](https://www.dmtf.org/sites/default/files/standards/documents/DSP0254_1.0.0.pdf).

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
  gManageabilityPkgTokenSpaceGuid.PcdMctpSourceEndpointId|9
  gManageabilityPkgTokenSpaceGuid.PcdMctpDestinationEndpointId|8
  gManageabilityPkgTokenSpaceGuid.PcdMctpKcsBaseAddress|0xca8
```
Couple of words about PCDs:
- The `PcdMctpSourceEndpointId` can be arbitrary (keep in mind that numbers 0-7 are reserved per specification),
- The `PcdMctpDestinationEndpointId` is mapped to the default `libmctp` EID (https://github.com/openbmc/libmctp/blob/2a2a0f6fd83318cfc37f44a657c9490c6a58a03a/utils/mctp-demux-daemon.c#L43),
- The `PcdMctpKcsBaseAddress` should be chosen based on your hardware settings and synced with the BMC settings. In this case I use `0xca8`, later this value would be seen in the BMC configuration.


Also the current code is not fully complaint with the [Management Component Transport Protocol (MCTP) KCS Transport Binding Specification](https://www.dmtf.org/sites/default/files/standards/documents/DSP0254_1.0.0.pdf).
As a temprarily workaround we would be using the following [fork](https://github.com/Kostr/edk2-platforms).

# BMC configuration

The MCTP communication in OpenBMC can be based on the
- userspace application ([libmctp](https://github.com/openbmc/libmctp) approach)
- kernel net mctp driver ([info](https://codeconstruct.com.au/docs/mctp-on-linux-introduction/), [current bindings](https://github.com/openbmc/linux/tree/dev-6.1/drivers/net/mctp))

Both approaches currently lack support for the MCTP over KCS binding, but the repository contains solutions for both methods.
- [userspace approach](mctp-userspace)
- [kernel approach](mctp-kernel)

The userspace solution is now considered legacy, if possible you should use kernel solution.

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
