#ifndef _Tools_
#define _Tools_

#if defined(__APPLE__)
#include "VideoMasterHD/VideoMasterHD_Core.h"
#include "VideoMasterHD/VideoMasterHD_Sdi.h"
#include "VideoMasterHD/VideoMasterHD_Asi.h"
#else
#include "VideoMasterHD_Core.h"
#include "VideoMasterHD_Sdi.h"
#include "VideoMasterHD_Asi.h"
#endif

const char* GetChnTypeName(VHD_CHANNELTYPE ChnType_E);
const char * GetErrorDescription(ULONG CodeError);
VHD_CORE_BOARDPROPERTY ChnIdx2BpChnType( BOOL32 Rx_B, int Idx_i );
VHD_STREAMTYPE GetRxStreamType( int Idx_i);
VHD_CORE_BOARDPROPERTY ChnIdx2BpStatus( BOOL32 Rx_B, int Idx_i );
VHD_STREAMTYPE GetTxStreamType( int Idx_i);
VHD_ASI_BITRATESOURCE GetBitRateSrc(int RxIdx_i);
void PrintChnType(HANDLE BoardHandle);
void PrintBoardInfo(int BoardIndex);
void WaitForChannelLocked(HANDLE BoardHandle, VHD_CORE_BOARDPROPERTY ChannelStatus_E);
void WaitForGenlockRef(HANDLE BoardHandle);
void PrintVideoStandardInfo(ULONG VideoStandard);
BOOL32 GetVideoCharacteristics(ULONG VideoStandard, int * pWidth, int * pHeight, BOOL32 * pInterlaced, BOOL32 * pIsHD);
BOOL32 IsKeyerAvailable(HANDLE BoardHandle);
const char * GetNameVideoStandard(ULONG VideoStandard);
void PrintInterfaceInfo(ULONG Interface);
const char * GetNameInterface(ULONG Interface);
int GetRawFrameHeight(ULONG VideoStandard);
BOOL32 SetNbChannels(ULONG BrdId, ULONG NbRx, ULONG NbTx);
BOOL32 Is4KInterface(ULONG Interface);
BOOL32 SingleToQuadLinksInterface( ULONG RXStatus, ULONG *pInterface, ULONG *pVideoStandard);
BOOL32 SingleToQuadLinksVideoStandard( ULONG *pVideoStandard);
BOOL32 GetFrameST2022_6PacketNumber(ULONG VideoStandard, ULONG *pPacketNumber);
BOOL32 GetCarriedVideoStandard(ULONG VideoStandard);
void *PageAlignedAlloc(ULONG Size);
void PageAlignedFree(void *pBuffer, ULONG Size);
#endif //_Tools_
