/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "pe.h"

#include <ntimage.h>

#include "nt.h"


_Use_decl_annotations_
EXTERN_C
NTSTATUS
PeGetSectionsByCharacteristics(
    ULONG_PTR ImageBase,
    ULONG Characteristics,
    PIMAGE_SECTION_HEADER** pppSectionHeaders,
    PULONG pnSectionHeaders
)
/*++

Routine Description:

    Returns the image section header pointer of each section in the image with
    the specified characteristics.

Parameters:

    ImageBase - The base address of the target image.

    Characteristics - A bitmask of image section characteristics to match
        against.

    pppSectionHeaders - Returns a pointer to an allocated array of image
        section header pointers for sections with the specified
        characteristics. The array is allocated from the NonPaged pool.

    pnSectionHeaders - Returns the number of elements in the allocated array.

Remarks:

    If successful, the caller must free the returned array by calling
    ExFreePool.

--*/
{
    PIMAGE_NT_HEADERS pNtHeaders = NULL;
    PIMAGE_SECTION_HEADER pSectionHeader = NULL;
    USHORT i = 0;
    ULONG nSectionHeaders = 0;
    SIZE_T cbSectionHeaders = 0;
    ULONG j = 0;
    PIMAGE_SECTION_HEADER* ppSectionHeaders = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    *pppSectionHeaders = NULL;
    *pnSectionHeaders = 0;

    pNtHeaders = RtlImageNtHeader((PVOID)ImageBase);
    if (!pNtHeaders)
    {
        ntstatus = STATUS_INVALID_IMAGE_FORMAT;
        goto exit;
    }

    //
    // Determine the number of sections which have the specified
    //  characteristics.
    //
    pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);

    for (i = 0;
        i < pNtHeaders->FileHeader.NumberOfSections;
        ++i, ++pSectionHeader)
    {
        if (pSectionHeader->Characteristics & Characteristics)
        {
            nSectionHeaders++;
        }
    }
    //
    if (!nSectionHeaders)
    {
        ntstatus = STATUS_NOT_FOUND;
        goto exit;
    }

    //
    // Allocate and initialize the returned array.
    //
    cbSectionHeaders = nSectionHeaders * sizeof(*ppSectionHeaders);

    ppSectionHeaders = (PIMAGE_SECTION_HEADER*)ExAllocatePool(
        NonPagedPool,
        cbSectionHeaders);
    if (!ppSectionHeaders)
    {
        ntstatus = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);

    for (i = 0, j = 0;
        i < pNtHeaders->FileHeader.NumberOfSections;
        ++i, ++pSectionHeader)
    {
        if (pSectionHeader->Characteristics & Characteristics)
        {
            ppSectionHeaders[j] = pSectionHeader;
            j++;
        }
    }

    //
    // Set out parameters.
    //
    *pppSectionHeaders = ppSectionHeaders;
    *pnSectionHeaders = nSectionHeaders;

exit:
    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
PeGetExecutableSections(
    ULONG_PTR ImageBase,
    PIMAGE_SECTION_HEADER** pppSectionHeaders,
    PULONG pnSectionHeaders
)
{
    return PeGetSectionsByCharacteristics(
        ImageBase,
        IMAGE_SCN_MEM_EXECUTE,
        pppSectionHeaders,
        pnSectionHeaders);
}
