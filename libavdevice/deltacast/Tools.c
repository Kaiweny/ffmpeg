#if !defined(__linux__) && !defined(__APPLE__)
#include <Windows.h>
#endif

#include <stdio.h>
#include "Tools.h"

#if defined (__linux__) || defined (__APPLE__)
//#include "Keyboard.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#define PAUSE(param) usleep(param*1000)
#else
#define PAUSE(param) Sleep(param)
#include <conio.h>
#define init_keyboard() 
#define close_keyboard()
#endif

const char* GetChnTypeName(VHD_CHANNELTYPE ChnType_E)
{
   switch(ChnType_E)
   {
   case VHD_CHNTYPE_DISABLE : return "Not Present";
   case VHD_CHNTYPE_SDSDI : return "SD-SDI";
   case VHD_CHNTYPE_HDSDI : return "HD-SDI";
   case VHD_CHNTYPE_3GSDI : return "3G-SDI";
   case VHD_CHNTYPE_DVI : return "DVI";
   case VHD_CHNTYPE_ASI : return "ASI";
   default : return "?";
   }
}

const char * GetErrorDescription(ULONG CodeError)
{
   switch (CodeError)
   {
   case VHDERR_NOERROR :                              return "No error";
   case VHDERR_FATALERROR :                           return "Fatal error occurred (should re-install)";
   case VHDERR_OPERATIONFAILED :                      return "Operation failed (undefined error)";
   case VHDERR_NOTENOUGHRESOURCE :                    return "Not enough resource to complete the operation";
   case VHDERR_NOTIMPLEMENTED :                       return "Not implemented yet";
   case VHDERR_NOTFOUND :                             return "Required element was not found";
   case VHDERR_BADARG :                               return "Bad argument value";
   case VHDERR_INVALIDPOINTER :                       return "Invalid pointer";
   case VHDERR_INVALIDHANDLE :                        return "Invalid handle";
   case VHDERR_INVALIDPROPERTY :                      return "Invalid property index";
   case VHDERR_INVALIDSTREAM :                        return "Invalid stream or invalid stream type";
   case VHDERR_RESOURCELOCKED :                       return "Resource is currently locked";
   case VHDERR_BOARDNOTPRESENT :                      return "Board is not available";
   case VHDERR_INCOHERENTBOARDSTATE :                 return "Incoherent board state or register value";
   case VHDERR_INCOHERENTDRIVERSTATE :                return "Incoherent driver state";
   case VHDERR_INCOHERENTLIBSTATE :                   return "Incoherent library state";
   case VHDERR_SETUPLOCKED :                          return "Configuration is locked";
   case VHDERR_CHANNELUSED :                          return "Requested channel is already used or doesn't exist";
   case VHDERR_STREAMUSED :                           return "Requested stream is already used";
   case VHDERR_READONLYPROPERTY :                     return "Property is read-only";
   case VHDERR_OFFLINEPROPERTY :                      return "Property is off-line only";
   case VHDERR_TXPROPERTY :                           return "Property is of TX streams";
   case VHDERR_TIMEOUT :                              return "Time-out occurred";
   case VHDERR_STREAMNOTRUNNING :                     return "Stream is not running";
   case VHDERR_BADINPUTSIGNAL :                       return "Bad input signal, or unsupported standard";
   case VHDERR_BADREFERENCESIGNAL :                   return "Bad genlock signal or unsupported standard";                                 
   case VHDERR_FRAMELOCKED :                          return "Frame already locked";
   case VHDERR_FRAMEUNLOCKED :                        return "Frame already unlocked";
   case VHDERR_INCOMPATIBLESYSTEM :                   return "Selected video standard is incompatible with running clock system";
   case VHDERR_ANCLINEISEMPTY :                       return "ANC line is empty";
   case VHDERR_ANCLINEISFULL :                        return "ANC line is full";
   case VHDERR_BUFFERTOOSMALL :                       return "Buffer too small";
   case VHDERR_BADANC :                               return "Received ANC aren't standard";
   case VHDERR_BADCONFIG :                            return "Invalid configuration";
   case VHDERR_FIRMWAREMISMATCH :                     return "The loaded firmware is not compatible with the installed driver";
   case VHDERR_LIBRARYMISMATCH :                      return "The loaded VideomasterHD library is not compatible with the installed driver";
   case VHDERR_FAILSAFE :                             return "The fail safe firmware is loaded. You need to upgrade your firmware";
   case VHDERR_RXPROPERTY :                           return "Property is of RX streams";
   case VHDERR_LTCSOURCEUNLOCKED :                    return "LTC source unlocked";
   case VHDERR_INVALIDACCESSRIGHT :                   return "Invalid access right";
   case VHDERR_LICENSERESTRICTION :                   return "Not allowed by the provided license";
   case VHDERR_SOFTWAREPROTECTION_FAILURE :           return "Error occured in the software protection module";
   case VHDERR_SOFTWAREPROTECTION_IDNOTFOUND :        return "Host ID cannot be found";
   case VHDERR_SOFTWAREPROTECTION_BADLICENSEINFO :    return "invalid provided License";
   case VHDERR_SOFTWAREPROTECTION_UNAUTHORIZEDHOST :  return "Host unauthorized";
   case VHDERR_SOFTWAREPROTECTION_STREAMSTARTED :     return "License providing requires all stream to be stopped";
   default:                                           return "Unknown code error";
   }
}

void PrintChnType(HANDLE BoardHandle)
{
   ULONG ChnType, NbOfChn;


   VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_NB_RXCHANNELS, &NbOfChn);
   for(ULONG i=0; i<NbOfChn; i++)
   {
      VHD_GetBoardProperty(BoardHandle, ChnIdx2BpChnType(TRUE,i), &ChnType);
      printf("RX%d=%s / ",i, GetChnTypeName((VHD_CHANNELTYPE)ChnType));
   }

   VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_NB_TXCHANNELS, &NbOfChn);
   for(ULONG i=0; i<NbOfChn; i++)
   {
      VHD_GetBoardProperty(BoardHandle, ChnIdx2BpChnType(FALSE,i), &ChnType);
      printf("TX%d=%s / ",i, GetChnTypeName((VHD_CHANNELTYPE)ChnType));
   }

   printf("\b\b\b   ");
}

VHD_CORE_BOARDPROPERTY ChnIdx2BpChnType( BOOL32 Rx_B, int Idx_i )
{
   switch(Idx_i)
   {
   case 0 : return (Rx_B?VHD_CORE_BP_RX0_TYPE:VHD_CORE_BP_TX0_TYPE);
   case 1 : return (Rx_B?VHD_CORE_BP_RX1_TYPE:VHD_CORE_BP_TX1_TYPE);
   case 2 : return (Rx_B?VHD_CORE_BP_RX2_TYPE:VHD_CORE_BP_TX2_TYPE);
   case 3 : return (Rx_B?VHD_CORE_BP_RX3_TYPE:VHD_CORE_BP_TX3_TYPE);
   case 4 : return (Rx_B?VHD_CORE_BP_RX4_TYPE:NB_VHD_CORE_BOARDPROPERTIES);
   case 5 : return (Rx_B?VHD_CORE_BP_RX5_TYPE:NB_VHD_CORE_BOARDPROPERTIES);
   case 6 : return (Rx_B?VHD_CORE_BP_RX6_TYPE:NB_VHD_CORE_BOARDPROPERTIES);
   case 7 : return (Rx_B?VHD_CORE_BP_RX7_TYPE:NB_VHD_CORE_BOARDPROPERTIES);
   default: return NB_VHD_CORE_BOARDPROPERTIES;
   }
}

VHD_STREAMTYPE GetRxStreamType( int Idx_i)
{
   switch(Idx_i)
   {
   case 0 : return VHD_ST_RX0;
   case 1 : return VHD_ST_RX1;
   case 2 : return VHD_ST_RX2;
   case 3 : return VHD_ST_RX3;
   case 4 : return VHD_ST_RX4;
   case 5 : return VHD_ST_RX5;
   case 6 : return VHD_ST_RX6;
   case 7 : return VHD_ST_RX7;
   default: return NB_VHD_STREAMTYPES;
   }
}

VHD_STREAMTYPE GetTxStreamType( int Idx_i)
{
   switch(Idx_i)
   {
   case 0 : return VHD_ST_TX0;
   case 1 : return VHD_ST_TX1;
   case 2 : return VHD_ST_TX2;
   case 3 : return VHD_ST_TX3;
   default: return NB_VHD_STREAMTYPES;
   }
}

VHD_ASI_BITRATESOURCE GetBitRateSrc(int RxIdx_i)
{
   switch(RxIdx_i)
   {
   case 0 : return VHD_ASI_BR_SRC_RX0;
   case 1 : return VHD_ASI_BR_SRC_RX1;
   default: return NB_VHD_ASI_BITRATESOURCE;      
   }
}

VHD_CORE_BOARDPROPERTY ChnIdx2BpStatus( BOOL32 Rx_B, int Idx_i )
{
   switch(Idx_i)
   {
   case 0 : return (Rx_B?VHD_CORE_BP_RX0_STATUS:VHD_CORE_BP_TX0_STATUS);
   case 1 : return (Rx_B?VHD_CORE_BP_RX1_STATUS:VHD_CORE_BP_TX1_STATUS);
   case 2 : return (Rx_B?VHD_CORE_BP_RX2_STATUS:VHD_CORE_BP_TX2_STATUS);
   case 3 : return (Rx_B?VHD_CORE_BP_RX3_STATUS:VHD_CORE_BP_TX3_STATUS);
   case 4 : return (Rx_B?VHD_CORE_BP_RX4_STATUS:NB_VHD_CORE_BOARDPROPERTIES);
   case 5 : return (Rx_B?VHD_CORE_BP_RX5_STATUS:NB_VHD_CORE_BOARDPROPERTIES);
   case 6 : return (Rx_B?VHD_CORE_BP_RX6_STATUS:NB_VHD_CORE_BOARDPROPERTIES);
   case 7 : return (Rx_B?VHD_CORE_BP_RX7_STATUS:NB_VHD_CORE_BOARDPROPERTIES);
   default: return NB_VHD_CORE_BOARDPROPERTIES;
   }
}

void PrintBoardInfo(int BoardIndex)
{
   ULONG             Result,DriverVersion,BoardType,SerialLsw,SerialMsw,SerialEx,NbOfLane;
   ULONG             FirmwareVersion,Firmware2Version,Firmware3Version,LowProfile,NbRxChannels,NbTxChannels,Firmware4Version;
   HANDLE            BoardHandle = NULL;

   /* Open a handle on first DELTA board */
   Result = VHD_OpenBoardHandle(BoardIndex,&BoardHandle,NULL,0);
   if (Result == VHDERR_NOERROR)
   {
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_DRIVER_VERSION, &DriverVersion);
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_FIRMWARE_VERSION, &FirmwareVersion);
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_BOARD_TYPE, &BoardType);
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_SERIALNUMBER_LSW, &SerialLsw);
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_SERIALNUMBER_MSW, &SerialMsw);
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_SERIALNUMBER_EX, &SerialEx);
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_NBOF_LANE, &NbOfLane);
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_FIRMWARE2_VERSION, &Firmware2Version);
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_LOWPROFILE, &LowProfile);
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_NB_RXCHANNELS, &NbRxChannels);
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_NB_TXCHANNELS, &NbTxChannels);

      printf("  Board %u :\n",BoardIndex);
      printf("    - Driver v%02d.%02d.%04d\n",DriverVersion>>24,(DriverVersion>>16)&0xFF,DriverVersion&0xFFFF);
      printf("    - Board fpga firmware v%02X (%02X-%02X-%02X)\n",FirmwareVersion&0xFF,(FirmwareVersion>>24)&0xFF,(FirmwareVersion>>16)&0xFF,(FirmwareVersion>>8)&0xFF);
      printf("    - Board cpld v%08X\n",Firmware2Version);
      if(BoardType==VHD_BOARDTYPE_DVI || BoardType==VHD_BOARDTYPE_3G || BoardType==VHD_BOARDTYPE_3GKEY || (BoardType==VHD_BOARDTYPE_HD && NbTxChannels==4))
      {
         VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_FIRMWARE3_VERSION, &Firmware3Version);
         printf("    - Board micro-controller firmware v%02X (%02X-%02X-%02X)\n",Firmware3Version&0xFF,(Firmware3Version>>24)&0xFF,(Firmware3Version>>16)&0xFF,(Firmware3Version>>8)&0xFF);
      }
      if(BoardType==VHD_BOARDTYPE_IP)
      {
         VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_FIRMWARE4_VERSION, &Firmware4Version);
         printf("    - Board microblaze firmware v%02X (%02X-%02X-%02X)\n",Firmware4Version&0xFF,(Firmware4Version>>24)&0xFF,(Firmware4Version>>16)&0xFF,(Firmware4Version>>8)&0xFF);
      }
      printf("    - Board serial# : 0x%08X%08X%08X\n",SerialEx,SerialMsw,SerialLsw);
      switch(BoardType)
      {
      case VHD_BOARDTYPE_HD :    printf("    - HD board type"); break;
      case VHD_BOARDTYPE_HDKEY : printf("    - HD key board type"); break;
      case VHD_BOARDTYPE_SD :    printf("    - SD board type"); break;
      case VHD_BOARDTYPE_SDKEY : printf("    - SD key board type"); break;
      case VHD_BOARDTYPE_DVI :   printf("    - DVI board type"); break;
      case VHD_BOARDTYPE_CODEC : printf("    - CODEC board type"); break;
      case VHD_BOARDTYPE_3G :    printf("    - 3G board type"); break;
		case VHD_BOARDTYPE_3GKEY : printf("    - 3G key board type"); break;
		case VHD_BOARDTYPE_ASI :   printf("    - ASI board type"); break;
		case VHD_BOARDTYPE_FLEX :  printf("    - FLEX board type"); break;
      case VHD_BOARDTYPE_HDMI:   printf("    - H4K board type"); break;
      case VHD_BOARDTYPE_IP:     printf("    - IP board type"); break;
      default :                  printf("    - Unknown board type"); break;
      }
      printf(" on %s bus",NbOfLane?"PCIe":"PCI");
      if(NbOfLane)
         printf(" (%d lane%s)\n",NbOfLane,(NbOfLane>1)?"s":"");
      else
         printf("\n");
      printf("    - %s\n",LowProfile?"Low profile":"Full height");
      printf("    - %d In / %d Out\n",NbRxChannels,NbTxChannels);

      printf("    - ");
      PrintChnType(BoardHandle);
      printf("\n");

      VHD_CloseBoardHandle(BoardHandle);
   }
   else
      printf("ERROR : Cannot open DELTA board %u handle. Result = 0x%08X (%s)\n",BoardIndex,Result, GetErrorDescription(Result));
}

void WaitForChannelLocked(HANDLE BoardHandle, VHD_CORE_BOARDPROPERTY ChannelStatus_E)
{
   ULONG Status = 0;
   ULONG Result = VHDERR_NOERROR;

   printf("\nWaiting for channel locked, press ESC to abort...\n");
   
   do 
   {
#if 0
      if (kbhit()) 
      {
         getch();
         break;
      }
 #endif     
      Result = VHD_GetBoardProperty(BoardHandle, ChannelStatus_E, &Status);
      PAUSE(100); 
      
      if (Result != VHDERR_NOERROR)
         continue;
      
   } while (Status & VHD_CORE_RXSTS_UNLOCKED);
}

void WaitForGenlockRef(HANDLE BoardHandle)
{
   ULONG Status = 0;
   ULONG Result = VHDERR_NOERROR;

   printf("\nWaiting for genlock reference, press ESC to abort...\n");

   do 
   {
#if 0
      if (kbhit()) 
      {
         getch();
         break;
      }
#endif
      Result = VHD_GetBoardProperty(BoardHandle, VHD_SDI_BP_GENLOCK_STATUS, &Status);
      PAUSE(100); 

      if (Result != VHDERR_NOERROR)
         continue;

   } while (Status & VHD_SDI_GNLKSTS_NOREF);
}

void PrintVideoStandardInfo(ULONG VideoStandard)
{
   printf("\nIncoming video standard : %s\n",GetNameVideoStandard(VideoStandard));
}

BOOL32 GetVideoCharacteristics(ULONG VideoStandard, int * pWidth, int * pHeight, BOOL32 * pInterlaced, BOOL32 * pIsHD)
{
   int Width = 0, Height = 0;
   BOOL32 Interlaced = FALSE, IsHD = FALSE;

   switch (VideoStandard)
   {
   case VHD_VIDEOSTD_S259M_NTSC: Width = 720; Height = 487; Interlaced = TRUE; IsHD = FALSE; break;
   case VHD_VIDEOSTD_S259M_PAL: Width = 720; Height = 576; Interlaced = TRUE; IsHD = FALSE; break;
   case VHD_VIDEOSTD_S296M_720p_24Hz:
   case VHD_VIDEOSTD_S296M_720p_25Hz:
   case VHD_VIDEOSTD_S296M_720p_30Hz:
   case VHD_VIDEOSTD_S296M_720p_50Hz:
   case VHD_VIDEOSTD_S296M_720p_60Hz: Width = 1280; Height = 720; Interlaced = FALSE; IsHD = TRUE; break;
   case VHD_VIDEOSTD_S274M_1080i_50Hz: 
   case VHD_VIDEOSTD_S274M_1080i_60Hz: 
   case VHD_VIDEOSTD_S274M_1080psf_24Hz:
   case VHD_VIDEOSTD_S274M_1080psf_25Hz:
   case VHD_VIDEOSTD_S274M_1080psf_30Hz: Width = 1920; Height = 1080; Interlaced = TRUE; IsHD = TRUE; break;
   case VHD_VIDEOSTD_S274M_1080p_25Hz:
   case VHD_VIDEOSTD_S274M_1080p_24Hz:
   case VHD_VIDEOSTD_S274M_1080p_30Hz:
   case VHD_VIDEOSTD_S274M_1080p_60Hz:
   case VHD_VIDEOSTD_S274M_1080p_50Hz: Width = 1920; Height = 1080; Interlaced = FALSE; IsHD = TRUE; break;
   case VHD_VIDEOSTD_S2048M_2048psf_24Hz:
   case VHD_VIDEOSTD_S2048M_2048psf_25Hz:
   case VHD_VIDEOSTD_S2048M_2048psf_30Hz: Width = 2048; Height = 1080; Interlaced = TRUE; IsHD = TRUE; break;
   case VHD_VIDEOSTD_S2048M_2048p_24Hz:
   case VHD_VIDEOSTD_S2048M_2048p_25Hz:
   case VHD_VIDEOSTD_S2048M_2048p_30Hz:
   case VHD_VIDEOSTD_S2048M_2048p_60Hz:
   case VHD_VIDEOSTD_S2048M_2048p_50Hz:
   case VHD_VIDEOSTD_S2048M_2048p_48Hz: Width = 2048; Height = 1080; Interlaced = FALSE; IsHD = TRUE; break;
   case VHD_VIDEOSTD_4096x2160p_24Hz:
   case VHD_VIDEOSTD_4096x2160p_25Hz:
   case VHD_VIDEOSTD_4096x2160p_30Hz:
   case VHD_VIDEOSTD_4096x2160p_60Hz:
   case VHD_VIDEOSTD_4096x2160p_50Hz:
   case VHD_VIDEOSTD_4096x2160p_48Hz: Width = 4096; Height = 2160; Interlaced = FALSE; IsHD = TRUE; break;
   case VHD_VIDEOSTD_3840x2160p_24Hz:
   case VHD_VIDEOSTD_3840x2160p_25Hz:
   case VHD_VIDEOSTD_3840x2160p_30Hz:
   case VHD_VIDEOSTD_3840x2160p_60Hz:
   case VHD_VIDEOSTD_3840x2160p_50Hz: Width = 3840; Height = 2160; Interlaced = FALSE; IsHD = TRUE; break;
   default: return FALSE;
   }

   if (pWidth) *pWidth = Width;
   if (pHeight) *pHeight = Height;
   if (pIsHD) *pIsHD = IsHD;
   if (pInterlaced) *pInterlaced = Interlaced;

   return TRUE;
}

BOOL32 IsKeyerAvailable(HANDLE BoardHandle)
{
   VHD_BOARDTYPE BoardType;

   ULONG Result = VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_BOARD_TYPE, (ULONG*)&BoardType);

   if (Result == VHDERR_NOERROR)
   {
      switch (BoardType)
      {
      case VHD_BOARDTYPE_SDKEY:
      case VHD_BOARDTYPE_HDKEY:
      case VHD_BOARDTYPE_3GKEY: return TRUE;
      default: break;
      }
   }
   return FALSE;
}

const char * GetNameVideoStandard(ULONG VideoStandard)
{
   switch (VideoStandard)
   {
   case VHD_VIDEOSTD_S259M_NTSC: return "SMPTE 259M - NTSC";
   case VHD_VIDEOSTD_S259M_PAL: return "SMPTE 259M - PAL"; 
   case VHD_VIDEOSTD_S274M_1080i_50Hz: return "SMPTE 274M - 1080i @ 50Hz"; 
   case VHD_VIDEOSTD_S274M_1080i_60Hz: return "SMPTE 274M - 1080i @ 60Hz"; 
   case VHD_VIDEOSTD_S274M_1080p_25Hz: return "SMPTE 274M - 1080p @ 25Hz"; 
   case VHD_VIDEOSTD_S274M_1080p_30Hz: return "SMPTE 274M - 1080p @ 30Hz"; 
   case VHD_VIDEOSTD_S296M_720p_50Hz: return "SMPTE 296M - 720p @ 50Hz"; 
   case VHD_VIDEOSTD_S296M_720p_60Hz: return "SMPTE 296M - 720p @ 60Hz"; 
   case VHD_VIDEOSTD_S274M_1080p_24Hz: return "SMPTE 274M - 1080p @ 24Hz";
   case VHD_VIDEOSTD_S274M_1080p_60Hz: return "SMPTE 274M - 1080p @ 60Hz";
   case VHD_VIDEOSTD_S274M_1080p_50Hz: return "SMPTE 274M - 1080p @ 50Hz";
   case VHD_VIDEOSTD_S274M_1080psf_24Hz: return "SMPTE 274M - 1080psf @ 24Hz";
   case VHD_VIDEOSTD_S274M_1080psf_25Hz: return "SMPTE 274M - 1080psf @ 25Hz";
   case VHD_VIDEOSTD_S274M_1080psf_30Hz: return "SMPTE 274M - 1080psf @ 30Hz";
   case VHD_VIDEOSTD_S296M_720p_24Hz: return "SMPTE 296M - 720p @ 24HZ";
   case VHD_VIDEOSTD_S296M_720p_25Hz: return "SMPTE 296M - 720p @ 25Hz";
   case VHD_VIDEOSTD_S296M_720p_30Hz: return "SMPTE 296M - 720p @ 30Hz";
   case VHD_VIDEOSTD_S2048M_2048p_24Hz: return "SMPTE 2048 - 2048p @ 24Hz";
   case VHD_VIDEOSTD_S2048M_2048p_25Hz: return "SMPTE 2048 - 2048p @ 25Hz";
   case VHD_VIDEOSTD_S2048M_2048p_30Hz: return "SMPTE 2048 - 2048p @ 30Hz";
   case VHD_VIDEOSTD_S2048M_2048psf_24Hz: return "SMPTE 2048 - 2048psf @ 24Hz";
   case VHD_VIDEOSTD_S2048M_2048psf_25Hz: return "SMPTE 2048 - 2048psf @ 25Hz";
   case VHD_VIDEOSTD_S2048M_2048psf_30Hz: return "SMPTE 2048 - 2048psf @ 30Hz";
   case VHD_VIDEOSTD_S2048M_2048p_60Hz: return "SMPTE 2048 - 2048p @ 60Hz";
   case VHD_VIDEOSTD_S2048M_2048p_50Hz: return "SMPTE 2048 - 2048p @ 50Hz";
   case VHD_VIDEOSTD_S2048M_2048p_48Hz: return "SMPTE 2048 - 2048p @ 48Hz";
   case VHD_VIDEOSTD_3840x2160p_24Hz: return "3840x2160p @ 24Hz";
   case VHD_VIDEOSTD_3840x2160p_25Hz: return "3840x2160p @ 25Hz";
   case VHD_VIDEOSTD_3840x2160p_30Hz: return "3840x2160p @ 30Hz";
   case VHD_VIDEOSTD_3840x2160p_50Hz: return "3840x2160p @ 50Hz";
   case VHD_VIDEOSTD_3840x2160p_60Hz: return "3840x2160p @ 60Hz";
   case VHD_VIDEOSTD_4096x2160p_24Hz: return "4096x2160p @ 24Hz";
   case VHD_VIDEOSTD_4096x2160p_25Hz: return "4096x2160p @ 25Hz";
   case VHD_VIDEOSTD_4096x2160p_30Hz: return "4096x2160p @ 30Hz";
   case VHD_VIDEOSTD_4096x2160p_48Hz: return "4096x2160p @ 48Hz";
   case VHD_VIDEOSTD_4096x2160p_50Hz: return "4096x2160p @ 50Hz";
   case VHD_VIDEOSTD_4096x2160p_60Hz: return "4096x2160p @ 60Hz";
   default: return "Unknown video standard";
   }
}

const char * GetNameInterface(ULONG Interface)
{
   switch(Interface)
   {
   case VHD_INTERFACE_DEPRECATED: return "Deprecated";
   case VHD_INTERFACE_SD_259: return "SD SMPTE 259";
   case VHD_INTERFACE_HD_292_1: return "HD SMPTE 292-1";
   case VHD_INTERFACE_HD_DUAL_372: return "HD-Dual SMPTE 372";
   case VHD_INTERFACE_3G_A_425_1: return "3G Level A SMPTE 425-1";
   case VHD_INTERFACE_4XHD: return "4xHD";
   case VHD_INTERFACE_4X3G_A: return "4x3G Level A";
   case VHD_INTERFACE_SD_DUAL: return "SD-Dual";
   case VHD_INTERFACE_3G_A_DUAL: return "3G Level A Dual";
   case VHD_INTERFACE_3G_B_425_1_DL: return "3G Level B Dual-Link";
   case VHD_INTERFACE_4X3G_B_DL_QUADRANT: return "4x3G Level B Dual-Link";  
   case VHD_INTERFACE_2X3G_B_DS_425_3: return "2x3G Level B Dual-Stream 425-3";     
   case VHD_INTERFACE_4X3G_A_425_5: return "4x3G Level A 425-5";        
   case VHD_INTERFACE_4X3G_B_DL_425_5: return "4x3G Level B Dual-Link 425-5"; 
   default: return "Unknown interface";
   }
}

void PrintInterfaceInfo(ULONG Interface)
{
   printf("\nIncoming interface : %s\n",GetNameInterface(Interface));
}

int GetFPS(ULONG VideoStandard) {
	int fps = 0;

	switch (VideoStandard)
	{
	  case VHD_VIDEOSTD_S259M_NTSC: fps = 0; break;
	  case VHD_VIDEOSTD_S259M_PAL: fps = 0; break;
	  case VHD_VIDEOSTD_S296M_720p_50Hz:
	  case VHD_VIDEOSTD_S274M_1080i_50Hz:
	  case VHD_VIDEOSTD_S274M_1080p_50Hz:
	  case VHD_VIDEOSTD_S2048M_2048p_50Hz: fps = 50; break;
	  case VHD_VIDEOSTD_S296M_720p_60Hz:
	  case VHD_VIDEOSTD_S274M_1080i_60Hz:
	  case VHD_VIDEOSTD_S274M_1080p_60Hz:
	  case VHD_VIDEOSTD_S2048M_2048p_60Hz: fps = 60; break;
	  case VHD_VIDEOSTD_S296M_720p_24Hz:
	  case VHD_VIDEOSTD_S274M_1080p_24Hz:
	  case VHD_VIDEOSTD_S274M_1080psf_24Hz:
	  case VHD_VIDEOSTD_S2048M_2048p_24Hz:
	  case VHD_VIDEOSTD_S2048M_2048psf_24Hz: fps = 24; break;
	  case VHD_VIDEOSTD_S296M_720p_25Hz:
	  case VHD_VIDEOSTD_S274M_1080p_25Hz:
	  case VHD_VIDEOSTD_S274M_1080psf_25Hz:
	  case VHD_VIDEOSTD_S2048M_2048p_25Hz:
	  case VHD_VIDEOSTD_S2048M_2048psf_25Hz: fps = 25; break;
	  case VHD_VIDEOSTD_S296M_720p_30Hz:
	  case VHD_VIDEOSTD_S274M_1080p_30Hz:
	  case VHD_VIDEOSTD_S274M_1080psf_30Hz:
	  case VHD_VIDEOSTD_S2048M_2048p_30Hz:
	  case VHD_VIDEOSTD_S2048M_2048psf_30Hz: fps = 30; break;
	  case VHD_VIDEOSTD_S2048M_2048p_48Hz: fps = 48; break;
	  default: fps = 0; break;
	}

	return fps;

}


int GetRawFrameHeight(ULONG VideoStandard)
{
	int RawFrameHeight=0;

	switch (VideoStandard)
	{
	case VHD_VIDEOSTD_S259M_NTSC: RawFrameHeight=525; break;
	case VHD_VIDEOSTD_S259M_PAL: RawFrameHeight=625; break;
	case VHD_VIDEOSTD_S296M_720p_50Hz:
	case VHD_VIDEOSTD_S296M_720p_60Hz:
	case VHD_VIDEOSTD_S296M_720p_24Hz:
	case VHD_VIDEOSTD_S296M_720p_25Hz:
	case VHD_VIDEOSTD_S296M_720p_30Hz: RawFrameHeight=750; break;
	case VHD_VIDEOSTD_S274M_1080i_50Hz:
	case VHD_VIDEOSTD_S274M_1080i_60Hz:
	case VHD_VIDEOSTD_S274M_1080p_25Hz:
	case VHD_VIDEOSTD_S274M_1080p_30Hz:
	case VHD_VIDEOSTD_S274M_1080p_24Hz:
	case VHD_VIDEOSTD_S274M_1080p_60Hz:
	case VHD_VIDEOSTD_S274M_1080p_50Hz:
	case VHD_VIDEOSTD_S274M_1080psf_24Hz:
	case VHD_VIDEOSTD_S274M_1080psf_25Hz:
	case VHD_VIDEOSTD_S274M_1080psf_30Hz:
	case VHD_VIDEOSTD_S2048M_2048p_24Hz:
	case VHD_VIDEOSTD_S2048M_2048p_25Hz:
	case VHD_VIDEOSTD_S2048M_2048p_30Hz:
	case VHD_VIDEOSTD_S2048M_2048psf_24Hz:
	case VHD_VIDEOSTD_S2048M_2048psf_25Hz:
	case VHD_VIDEOSTD_S2048M_2048psf_30Hz:
	case VHD_VIDEOSTD_S2048M_2048p_60Hz:
	case VHD_VIDEOSTD_S2048M_2048p_50Hz:
	case VHD_VIDEOSTD_S2048M_2048p_48Hz: RawFrameHeight=1125; break;
	default: RawFrameHeight=0; break;
	}

	return RawFrameHeight;
}

BOOL32 SetNbChannels(ULONG BrdId, ULONG NbRx, ULONG NbTx)
{
   BOOL32 Result = FALSE;
   ULONG VhdResult = VHDERR_NOERROR;
   ULONG NbRxOnBoard = 0, NbTxOnBoard = 0, NbChanOnBoard = 0; 
   HANDLE BoardHandle = NULL;
   BOOL32 IsBiDir = FALSE;

   VhdResult = VHD_OpenBoardHandle(BrdId,&BoardHandle,NULL,0);
   if (VhdResult == VHDERR_NOERROR)
   {
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_NB_RXCHANNELS, &NbRxOnBoard);
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_NB_TXCHANNELS, &NbTxOnBoard);
      VHD_GetBoardProperty(BoardHandle, VHD_CORE_BP_IS_BIDIR, (ULONG*)&IsBiDir);

      VHD_CloseBoardHandle(BoardHandle);

      NbChanOnBoard = NbRxOnBoard + NbTxOnBoard;

      if ((NbRxOnBoard >= NbRx) && (NbTxOnBoard >= NbTx))
         Result = TRUE;    
   
      if (!Result)
      {
         if (IsBiDir)
         {
            if ((NbRx+NbTx)<=NbChanOnBoard)
            {
               printf("\nChanging board configuration... ");

               if (NbChanOnBoard == 2)
               {
                  switch (NbRx)
                  {
                  case 0 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_02);break;
                  case 1 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_11);break;
                  case 2 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_20);break;
                  }
               }
               else if (NbChanOnBoard == 4)
               {
                  switch (NbRx)
                  {
                  case 0 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_04);break;
                  case 1 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_13);break;
                  case 2 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_22);break;
                  case 3 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_31);break;
                  case 4 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_40);break;
                  }
               }
               else if (NbChanOnBoard == 8)
               {
                  switch (NbRx)
                  {
                  case 0 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_08);break;
                  case 1 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_17);break;
                  case 2 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_26);break;
                  case 3 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_35);break;
                  case 4 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_44);break;
                  case 5 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_53);break;
                  case 6 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_62);break;
                  case 7 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_71);break;
                  case 8 :	VHD_SetBiDirCfg(BrdId, VHD_BIDIR_80);break;
                  }
               }

               Result = TRUE;    

               printf("Set configuration %d In / %d Out\n", NbRx, NbChanOnBoard-NbRx);
            }         
         }
      }
   }
   
   return Result;   
}

BOOL32 Is4KInterface(ULONG Interface)
{
   BOOL32 Result = FALSE;
   switch (Interface)
   {
   case VHD_INTERFACE_4XHD: 
   case VHD_INTERFACE_4X3G_A: 
   case VHD_INTERFACE_4X3G_B_DL_QUADRANT: 
   case VHD_INTERFACE_2X3G_B_DS_425_3: 
   case VHD_INTERFACE_4X3G_A_425_5:
   case VHD_INTERFACE_4X3G_B_DL_425_5: Result = TRUE; break;
   }

   return Result;
}

BOOL32 SingleToQuadLinksInterface( ULONG RXStatus, ULONG *pInterface, ULONG *pVideoStandard)
{
   BOOL32 Result = TRUE;

   if(pInterface && pVideoStandard)
   {
      switch(*pVideoStandard)
      {
      case VHD_VIDEOSTD_S274M_1080p_24Hz: *pVideoStandard = VHD_VIDEOSTD_3840x2160p_24Hz;
         *pInterface = VHD_INTERFACE_4XHD; break;
      case VHD_VIDEOSTD_S274M_1080p_25Hz: *pVideoStandard = VHD_VIDEOSTD_3840x2160p_25Hz;
         *pInterface = VHD_INTERFACE_4XHD; break;
      case VHD_VIDEOSTD_S274M_1080p_30Hz: *pVideoStandard = VHD_VIDEOSTD_3840x2160p_30Hz;
         *pInterface = VHD_INTERFACE_4XHD; break;
      case VHD_VIDEOSTD_S274M_1080p_50Hz: *pVideoStandard = VHD_VIDEOSTD_3840x2160p_50Hz;
         if(RXStatus&VHD_SDI_RXSTS_LEVELB_3G)
            *pInterface = VHD_INTERFACE_4X3G_B_DL;
         else
            *pInterface = VHD_INTERFACE_4X3G_A;
         break;
      case VHD_VIDEOSTD_S274M_1080p_60Hz:	*pVideoStandard = VHD_VIDEOSTD_3840x2160p_60Hz;
         if(RXStatus&VHD_SDI_RXSTS_LEVELB_3G)
            *pInterface = VHD_INTERFACE_4X3G_B_DL;
         else
            *pInterface = VHD_INTERFACE_4X3G_A;
         break;
      case VHD_VIDEOSTD_S2048M_2048p_24Hz:	*pVideoStandard = VHD_VIDEOSTD_4096x2160p_24Hz;
         *pInterface = VHD_INTERFACE_4XHD; break;
      case VHD_VIDEOSTD_S2048M_2048p_25Hz:	*pVideoStandard = VHD_VIDEOSTD_4096x2160p_25Hz;
         *pInterface = VHD_INTERFACE_4XHD; break;
      case VHD_VIDEOSTD_S2048M_2048p_30Hz:	*pVideoStandard = VHD_VIDEOSTD_4096x2160p_30Hz;
         *pInterface = VHD_INTERFACE_4XHD; break;
      case VHD_VIDEOSTD_S2048M_2048p_48Hz:	*pVideoStandard = VHD_VIDEOSTD_4096x2160p_48Hz;
         if(RXStatus&VHD_SDI_RXSTS_LEVELB_3G)
            *pInterface = VHD_INTERFACE_4X3G_B_DL;
         else
            *pInterface = VHD_INTERFACE_4X3G_A;
         break;
      case VHD_VIDEOSTD_S2048M_2048p_50Hz:	*pVideoStandard = VHD_VIDEOSTD_4096x2160p_50Hz;
         if(RXStatus&VHD_SDI_RXSTS_LEVELB_3G)
            *pInterface = VHD_INTERFACE_4X3G_B_DL;
         else
            *pInterface = VHD_INTERFACE_4X3G_A;
         break;
      case VHD_VIDEOSTD_S2048M_2048p_60Hz:	*pVideoStandard = VHD_VIDEOSTD_4096x2160p_60Hz;
         if(RXStatus&VHD_SDI_RXSTS_LEVELB_3G)
            *pInterface = VHD_INTERFACE_4X3G_B_DL;
         else
            *pInterface = VHD_INTERFACE_4X3G_A;
         break;
      default: Result = FALSE; break;
      }
   }
   else
      Result = FALSE;

   return Result;
}

BOOL32 SingleToQuadLinksVideoStandard( ULONG *pVideoStandard)
{
   BOOL32 Result = TRUE;

   if(pVideoStandard)
   {
      switch(*pVideoStandard)
      {
      case VHD_VIDEOSTD_S274M_1080p_24Hz: *pVideoStandard = VHD_VIDEOSTD_3840x2160p_24Hz; break;
      case VHD_VIDEOSTD_S274M_1080p_25Hz: *pVideoStandard = VHD_VIDEOSTD_3840x2160p_25Hz; break;
      case VHD_VIDEOSTD_S274M_1080p_30Hz: *pVideoStandard = VHD_VIDEOSTD_3840x2160p_30Hz; break;
      case VHD_VIDEOSTD_S274M_1080p_50Hz: *pVideoStandard = VHD_VIDEOSTD_3840x2160p_50Hz; break;
      case VHD_VIDEOSTD_S274M_1080p_60Hz:	*pVideoStandard = VHD_VIDEOSTD_3840x2160p_60Hz; break;
      case VHD_VIDEOSTD_S2048M_2048p_24Hz:	*pVideoStandard = VHD_VIDEOSTD_4096x2160p_24Hz; break;
      case VHD_VIDEOSTD_S2048M_2048p_25Hz:	*pVideoStandard = VHD_VIDEOSTD_4096x2160p_25Hz; break;
      case VHD_VIDEOSTD_S2048M_2048p_30Hz:	*pVideoStandard = VHD_VIDEOSTD_4096x2160p_30Hz; break;
      case VHD_VIDEOSTD_S2048M_2048p_48Hz:	*pVideoStandard = VHD_VIDEOSTD_4096x2160p_48Hz; break;
      case VHD_VIDEOSTD_S2048M_2048p_50Hz:	*pVideoStandard = VHD_VIDEOSTD_4096x2160p_50Hz; break;
      case VHD_VIDEOSTD_S2048M_2048p_60Hz:	*pVideoStandard = VHD_VIDEOSTD_4096x2160p_60Hz; break;
      default: Result = FALSE; break;
      }
   }
   else
      Result = FALSE;

   return Result;
}

BOOL32 GetFrameST2022_6PacketNumber(ULONG VideoStandard, ULONG *pPacketNumber)
{
   ULONG PacketNumber = 0;

   switch (VideoStandard)
   {
   case VHD_VIDEOSTD_S259M_NTSC:PacketNumber=819 ;break;
   case VHD_VIDEOSTD_S259M_PAL:PacketNumber=982 ;break;
   case VHD_VIDEOSTD_S296M_720p_60Hz:PacketNumber=2249 ;break;
   case VHD_VIDEOSTD_S296M_720p_50Hz:PacketNumber=2699 ;break;
   case VHD_VIDEOSTD_S274M_1080i_50Hz:PacketNumber= 5397; break;
   case VHD_VIDEOSTD_S274M_1080i_60Hz: PacketNumber= 4497; break;
   case VHD_VIDEOSTD_S274M_1080psf_25Hz:PacketNumber= 5397; break;
   case VHD_VIDEOSTD_S274M_1080psf_30Hz: PacketNumber= 4497; break;
   case VHD_VIDEOSTD_S274M_1080p_25Hz:PacketNumber= 5397;break;
   case VHD_VIDEOSTD_S274M_1080p_30Hz: PacketNumber= 4497;break;
   case VHD_VIDEOSTD_S274M_1080p_50Hz:PacketNumber= 5397; break;
   case VHD_VIDEOSTD_S274M_1080p_60Hz: PacketNumber= 4497; break;
   case VHD_VIDEOSTD_S274M_1080p_24Hz:PacketNumber= 5621;break;
   case VHD_VIDEOSTD_S274M_1080psf_24Hz:PacketNumber= 5621;break;
   default: return FALSE;
   }

   if (pPacketNumber) *pPacketNumber = PacketNumber;

   return TRUE;
}



