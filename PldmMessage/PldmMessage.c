/*
 * Copyright (c) 2023, Konstantin Aladyshev <aladyshev22@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/ManageabilityTransportLib.h>
#include <Library/ManageabilityTransportHelperLib.h>
#include <Library/ManageabilityTransportMctpLib.h>
#include <IndustryStandard/Mctp.h>
#include <IndustryStandard/Pldm.h>
#include <Protocol/MctpProtocol.h>

static const UINT8 PldmType = PLDM_TYPE_MESSAGE_CONTROL_AND_DISCOVERY;
static const UINT8 PldmTypeCommandCode = 0x04;

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS             Status;
  EDKII_MCTP_PROTOCOL* MctpProtocol;
  Status = gBS->LocateProtocol(&gEdkiiMctpProtocolGuid,
                               NULL, 
                               (VOID**)&MctpProtocol);
  if (EFI_ERROR(Status)) {
    Print(L"ERROR: Could not find MCTP protocol: %r\n", Status);
    return Status;
  }
  Print(L"MCTP protocol version %d\n", MctpProtocol->ProtocolVersion);

  PLDM_MESSAGE_HEADER PldmHeader;
  PldmHeader.RequestBit = PLDM_MESSAGE_HEADER_IS_REQUEST;
  PldmHeader.DatagramBit = (!PLDM_MESSAGE_HEADER_IS_DATAGRAM);
  PldmHeader.Reserved = 0;
  PldmHeader.InstanceId = 0x15;
  PldmHeader.HeaderVersion = PLDM_MESSAGE_HEADER_VERSION;
  PldmHeader.PldmType = PldmType;
  PldmHeader.PldmTypeCommandCode = PldmTypeCommandCode;

  MANAGEABILITY_TRANSPORT_ADDITIONAL_STATUS AdditionalTransferError;

  for (UINT8 i=0; i<sizeof(PldmHeader); i++) {
    Print(L"PldmRequestBuffer[%d]=0x%02x\n", i, ((UINT8*)&PldmHeader)[i]);
  }

  UINT8 Response[50];
  UINT32 ResponseDataSize = sizeof(Response);
  for (UINT8 i=0; i<ResponseDataSize; i++) {
    Response[i]=0xAA;	// data poison for debug purposes
  }

  Status = MctpProtocol->Functions.Version1_0->MctpSubmitCommand(
    MctpProtocol,
    MCTP_MESSAGE_TYPE_PLDM,
    9,
    8,
    FALSE,
    (UINT8*)&PldmHeader,
    sizeof(PldmHeader),
    10000,
    Response,
    &ResponseDataSize,
    10000,
    &AdditionalTransferError
  );

  if (EFI_ERROR(Status)) {
    Print(L"MctpSubmitCommand returned %r, additional status = 0x%08x\n", Status, AdditionalTransferError);
    return Status;
  }

//  // DEBUG
//  Print(L"ResponseDataSize=%d\n", ResponseDataSize);
//  for (UINT8 i=0; i<ResponseDataSize; i++) {
//    Print(L"Response[%d]=0x%02x\n", i, Response[i]);
//  }

  PLDM_RESPONSE_HEADER* PldmResponseHeader = (PLDM_RESPONSE_HEADER*)Response;
  if (ResponseDataSize < sizeof(*PldmResponseHeader)) {
    Print(L"Error! ResponseDataSize is less than PLDM_RESPONSE_HEADER\n");
    return EFI_DEVICE_ERROR;
  }
  if (PldmResponseHeader->PldmHeader.RequestBit != (!PLDM_MESSAGE_HEADER_IS_REQUEST)) {
    Print(L"Error! PldmHeader.RequestBit = %d, but should be %d\n",
            PldmResponseHeader->PldmHeader.RequestBit,
            (!PLDM_MESSAGE_HEADER_IS_REQUEST));
    return EFI_DEVICE_ERROR;
  }
  if (PldmResponseHeader->PldmHeader.DatagramBit != (!PLDM_MESSAGE_HEADER_IS_DATAGRAM)) {
    Print(L"Error! PldmHeader.DatagramBit = %d, but should be %d\n",
            PldmResponseHeader->PldmHeader.DatagramBit,
            (!PLDM_MESSAGE_HEADER_IS_DATAGRAM));
    return EFI_DEVICE_ERROR;
  }
  if (PldmResponseHeader->PldmHeader.InstanceId != PldmHeader.InstanceId) {
    Print(L"Error! PldmHeader.InstanceId = 0x%02x, but should be 0x%02x\n",
            PldmResponseHeader->PldmHeader.InstanceId,
            PldmHeader.InstanceId);
    return EFI_DEVICE_ERROR;
  }
  if (PldmResponseHeader->PldmHeader.HeaderVersion != PLDM_MESSAGE_HEADER_VERSION) {
    Print(L"Error! PldmHeader.HeaderVersion = %d, but should be %d\n",
            PldmResponseHeader->PldmHeader.HeaderVersion,
            PLDM_MESSAGE_HEADER_VERSION);
    return EFI_DEVICE_ERROR;
  }
  if (PldmResponseHeader->PldmHeader.PldmType != PldmHeader.PldmType) {
    Print(L"Error! PldmHeader.PldmType = %d, but should be %d\n",
            PldmResponseHeader->PldmHeader.PldmType,
            PldmHeader.PldmType);
    return EFI_DEVICE_ERROR;
  }
  if (PldmResponseHeader->PldmHeader.PldmTypeCommandCode != PldmHeader.PldmTypeCommandCode) {
    Print(L"Error! PldmHeader.PldmTypeCommandCode = %d, but should be %d\n",
            PldmResponseHeader->PldmHeader.PldmTypeCommandCode,
            PldmHeader.PldmTypeCommandCode);
    return EFI_DEVICE_ERROR;
  }
  if (PldmResponseHeader->PldmCompletionCode != PLDM_COMPLETION_CODE_SUCCESS) {
    Print(L"Error! PldmCompletionCode = 0x%02x, but should be 0x%02x\n",
            PldmResponseHeader->PldmCompletionCode,
            PLDM_COMPLETION_CODE_SUCCESS);
    return EFI_DEVICE_ERROR;
  }

  for (UINT8 i=sizeof(*PldmResponseHeader); i<ResponseDataSize; i++) {
    Print(L"PldmResponse[%d]=0x%02x\n", i-sizeof(*PldmResponseHeader), Response[i]);
  }

  return EFI_SUCCESS;
}
