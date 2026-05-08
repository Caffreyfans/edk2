#include "MetaXGpu.h"

#define MXGPU_EDID_BASE_BLOCK_SIZE 256
#define MXGPU_EDID_DTD0_OFFSET 54
#define MXGPU_EDID_DTD_SIZE 18

#define MXGPU_FALLBACK_WIDTH 1024
#define MXGPU_FALLBACK_HEIGHT 768

typedef struct
{
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    UINT32 RefreshRate;
    UINT32 PixelClockKHz;
} MXGPU_EDID_TIMING;

STATIC CONST UINT8 mMxgpuEdidHeader[8] = {
    0x00, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x00};

static BOOLEAN
MXgpuValidateBaseEdid(
    IN CONST UINT8 *Edid,
    IN UINT32 EdidSize)
{
    BOOLEAN Result;
    UINT32 Index;
    UINT8 Sum;

    Result = FALSE;
    Sum = 0;

    if (Edid == NULL)
    {
        DEBUG((DEBUG_ERROR, "MXGPU EDID: Edid is NULL\n"));
        goto Exit;
    }

    if (EdidSize < MXGPU_EDID_BASE_BLOCK_SIZE)
    {
        DEBUG((DEBUG_ERROR, "MXGPU EDID: EdidSize too small: %u\n", EdidSize));
        goto Exit;
    }

    if (CompareMem(Edid, mMxgpuEdidHeader, sizeof(mMxgpuEdidHeader)) != 0)
    {
        DEBUG((
            DEBUG_ERROR,
            "MXGPU EDID: invalid header: %02x %02x %02x %02x %02x %02x %02x %02x\n",
            Edid[0],
            Edid[1],
            Edid[2],
            Edid[3],
            Edid[4],
            Edid[5],
            Edid[6],
            Edid[7]));
        goto Exit;
    }

    for (Index = 0; Index < MXGPU_EDID_BASE_BLOCK_SIZE; Index++)
    {
        Sum = (UINT8)(Sum + Edid[Index]);
    }

    if (Sum != 0)
    {
        DEBUG((DEBUG_ERROR, "MXGPU EDID: base block checksum failed, sum=0x%02x\n", Sum));
        goto Exit;
    }

    Result = TRUE;

Exit:
    return Result;
}

BOOLEAN
MXgpuParsePreferredTiming(
    IN CONST UINT8 *Edid,
    IN UINT32 EdidSize,
    OUT MXGPU_EDID_TIMING *Timing)
{
    BOOLEAN Result;
    CONST UINT8 *Dtd;
    UINT32 PixelClock10KHz;
    UINT32 HActive;
    UINT32 HBlank;
    UINT32 VActive;
    UINT32 VBlank;
    UINT32 HTotal;
    UINT32 VTotal;
    UINT64 PixelClockHz;
    UINT64 Refresh;

    Result = FALSE;
    Dtd = NULL;
    PixelClock10KHz = 0;
    HActive = 0;
    HBlank = 0;
    VActive = 0;
    VBlank = 0;
    HTotal = 0;
    VTotal = 0;
    PixelClockHz = 0;
    Refresh = 0;

    if (Edid == NULL || Timing == NULL)
    {
        goto Exit;
    }

    if (EdidSize < MXGPU_EDID_BASE_BLOCK_SIZE)
    {
        goto Exit;
    }

    ZeroMem(Timing, sizeof(*Timing));

    Dtd = Edid + MXGPU_EDID_DTD0_OFFSET;

    //
    // Pixel clock is in 10 kHz units.
    // If it is 0, this DTD block is not a detailed timing descriptor.
    //
    PixelClock10KHz = (UINT32)Dtd[0] | ((UINT32)Dtd[1] << 8);
    if (PixelClock10KHz == 0)
    {
        DEBUG((DEBUG_WARN, "MXGPU EDID: DTD0 is not a timing descriptor\n"));
        goto Exit;
    }

    //
    // Horizontal active:
    //   DTD[2]             = low 8 bits
    //   DTD[4] bits [7:4] = high 4 bits
    //
    HActive = (UINT32)Dtd[2] | (((UINT32)Dtd[4] & 0xF0) << 4);

    //
    // Horizontal blanking:
    //   DTD[3]             = low 8 bits
    //   DTD[4] bits [3:0] = high 4 bits
    //
    HBlank = (UINT32)Dtd[3] | (((UINT32)Dtd[4] & 0x0F) << 8);

    //
    // Vertical active:
    //   DTD[5]             = low 8 bits
    //   DTD[7] bits [7:4] = high 4 bits
    //
    VActive = (UINT32)Dtd[5] | (((UINT32)Dtd[7] & 0xF0) << 4);

    //
    // Vertical blanking:
    //   DTD[6]             = low 8 bits
    //   DTD[7] bits [3:0] = high 4 bits
    //
    VBlank = (UINT32)Dtd[6] | (((UINT32)Dtd[7] & 0x0F) << 8);

    HTotal = HActive + HBlank;
    VTotal = VActive + VBlank;

    if (HActive == 0 || VActive == 0 || HTotal == 0 || VTotal == 0)
    {
        DEBUG((
            DEBUG_ERROR,
            "MXGPU EDID: invalid preferred timing, HActive=%u VActive=%u HTotal=%u VTotal=%u\n",
            HActive,
            VActive,
            HTotal,
            VTotal));
        goto Exit;
    }

    Timing->HorizontalResolution = HActive;
    Timing->VerticalResolution = VActive;
    Timing->PixelClockKHz = PixelClock10KHz * 10;

    PixelClockHz = MultU64x32((UINT64)Timing->PixelClockKHz, 1000);
    Refresh = DivU64x32(PixelClockHz, HTotal * VTotal);

    Timing->RefreshRate = (UINT32)Refresh;

    DEBUG((
        DEBUG_INFO,
        "MXGPU EDID: preferred timing %ux%u@%u, PixelClock=%u kHz, HTotal=%u, VTotal=%u\n",
        Timing->HorizontalResolution,
        Timing->VerticalResolution,
        Timing->RefreshRate,
        Timing->PixelClockKHz,
        HTotal,
        VTotal));

    Result = TRUE;

Exit:
    return Result;
}

BOOLEAN
MXgpuFindModeByResolution(
    IN CONST METAX_VIDEO_MODE_DATA *ModeTable,
    IN UINT32 ModeCount,
    IN UINT32 HorizontalResolution,
    IN UINT32 VerticalResolution,
    OUT UINT32 *ModeNumber)
{
    BOOLEAN Result;
    UINT32 Index;

    Result = FALSE;
    Index = 0;

    if (ModeTable == NULL || ModeNumber == NULL || ModeCount == 0)
    {
        goto Exit;
    }

    for (Index = 0; Index < ModeCount; Index++)
    {
        if (ModeTable[Index].HorizontalResolution == HorizontalResolution &&
            ModeTable[Index].VerticalResolution == VerticalResolution)
        {
            *ModeNumber = Index;
            Result = TRUE;
            break;
        }
    }

Exit:
    return Result;
}

UINT32
MxgpuChooseClosestMode(
    IN CONST METAX_VIDEO_MODE_DATA *ModeTable,
    IN UINT32 ModeCount,
    IN UINT32 PreferredWidth,
    IN UINT32 PreferredHeight)
{
    UINT32 Index;
    UINT32 BestIndex;
    UINT64 BestPixels;
    UINT64 Pixels;

    Index = 0;
    BestIndex = 0;
    BestPixels = 0;
    Pixels = 0;

    if (ModeTable == NULL || ModeCount == 0)
    {
        goto Exit;
    }

    for (Index = 0; Index < ModeCount; Index++)
    {
        if (ModeTable[Index].HorizontalResolution > PreferredWidth ||
            ModeTable[Index].VerticalResolution > PreferredHeight)
        {
            continue;
        }

        Pixels = (UINT64)ModeTable[Index].HorizontalResolution *
                 (UINT64)ModeTable[Index].VerticalResolution;

        if (Pixels > BestPixels)
        {
            BestPixels = Pixels;
            BestIndex = Index;
        }
    }

Exit:
    return BestIndex;
}

UINT32
MetaXgpuChooseDefaultModeFromEdid(
    IN CONST UINT8 *Edid,
    IN UINT32 EdidSize,
    IN CONST METAX_VIDEO_MODE_DATA *ModeTable,
    IN UINT32 ModeCount)
{
    MXGPU_EDID_TIMING Timing;
    UINT32 ModeNumber;
    UINT32 SelectedMode;
    BOOLEAN Found;

    ModeNumber = 0;
    SelectedMode = 0;
    Found = FALSE;

    ZeroMem(&Timing, sizeof(Timing));

    if (ModeTable == NULL || ModeCount == 0)
    {
        goto Exit;
    }

    if (MXgpuValidateBaseEdid(Edid, EdidSize))
    {
        if (MXgpuParsePreferredTiming(Edid, EdidSize, &Timing))
        {

            Found = MXgpuFindModeByResolution(
                ModeTable,
                ModeCount,
                Timing.HorizontalResolution,
                Timing.VerticalResolution,
                &ModeNumber);

            if (Found)
            {
                SelectedMode = ModeNumber;

                DEBUG((
                    DEBUG_INFO,
                    "MXGPU EDID: exact preferred mode found: Mode=%u, %ux%u\n",
                    SelectedMode,
                    ModeTable[SelectedMode].HorizontalResolution,
                    ModeTable[SelectedMode].VerticalResolution));

                goto Exit;
            }

            SelectedMode = MxgpuChooseClosestMode(
                ModeTable,
                ModeCount,
                Timing.HorizontalResolution,
                Timing.VerticalResolution);

            DEBUG((
                DEBUG_INFO,
                "MXGPU EDID: preferred %ux%u not found, choose closest Mode=%u, %ux%u\n",
                Timing.HorizontalResolution,
                Timing.VerticalResolution,
                SelectedMode,
                ModeTable[SelectedMode].HorizontalResolution,
                ModeTable[SelectedMode].VerticalResolution));

            goto Exit;
        }
    }

    Found = MXgpuFindModeByResolution(
        ModeTable,
        ModeCount,
        MXGPU_FALLBACK_WIDTH,
        MXGPU_FALLBACK_HEIGHT,
        &ModeNumber);

    if (Found)
    {
        SelectedMode = ModeNumber;

        DEBUG((
            DEBUG_WARN,
            "MXGPU EDID: fallback to Mode=%u, %ux%u\n",
            SelectedMode,
            MXGPU_FALLBACK_WIDTH,
            MXGPU_FALLBACK_HEIGHT));

        goto Exit;
    }

    SelectedMode = 0;

    DEBUG((DEBUG_WARN, "MXGPU EDID: fallback to Mode=0\n"));

Exit:
    return SelectedMode;
}