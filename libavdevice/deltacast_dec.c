#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include "libavutil/opt.h"

#include "VideoMasterHD_Core.h"
#include "VideoMasterHD_Sdi.h"
#include "VideoMasterHD_Sdi_Vbi.h"
#include "VideoMasterHD_Sdi_VbiData.h"

#include "libavdevice/deltacast/Tools.h"
#include "libavdevice/deltacast/Tools.h"

struct deltacast_ctx {
    AVClass *class;
    /* Deltacast SDK interfaces */
	HANDLE BoardHandle, StreamHandle, StreamHandleANC, SlotHandle;
    ULONG ChnType, VideoStandard, Interface, ClockSystem;	

    /* Deltacast mode information */

    /* Streams present */
    int audio;
    int video;

    /* Status */
    int capture_started;
    ULONG frameCount;
    ULONG dropped;
 //   AVStream *audio_st;


    AVStream *video_st;
	int width;
	int height;
	BOOL32 interlaced;
	BOOL32 isHD;
    int fps;
	
	

    /* Options */
    /* video channel index */	
    int v_channelIndex;

    /* Afd Slot AR Code */   
    int afd_ARCode;
};

static int start_video_stream(struct deltacast_ctx *ctx) {
	
	int status = 1;
    ULONG Result,Result1,DllVersion,NbBoards;
    ULONG BrdId = 0;
    ULONG NbRxRequired, NbTxRequired;       

	VHD_CORE_BOARDPROPERTY CHNTYPE;
	VHD_CORE_BOARDPROPERTY CHNSTATUS;
	VHD_SDI_BOARDPROPERTY CLOCKDIV;
    VHD_STREAMTYPE STRMTYPE;

	int ind = ctx->v_channelIndex;	

	switch(ind) {
		case 0:
			CHNTYPE = VHD_CORE_BP_RX0_TYPE; 
			CHNSTATUS = VHD_CORE_BP_RX0_STATUS;
			CLOCKDIV = VHD_SDI_BP_RX0_CLOCK_DIV;
			STRMTYPE = VHD_ST_RX0;		
		break;
		case 1:
			CHNTYPE = VHD_CORE_BP_RX1_TYPE; 
			CHNSTATUS = VHD_CORE_BP_RX1_STATUS;
			CLOCKDIV = VHD_SDI_BP_RX1_CLOCK_DIV;
			STRMTYPE = VHD_ST_RX1;		
		break;
		case 2:
			CHNTYPE = VHD_CORE_BP_RX2_TYPE; 
			CHNSTATUS = VHD_CORE_BP_RX2_STATUS;
			CLOCKDIV = VHD_SDI_BP_RX2_CLOCK_DIV;
			STRMTYPE = VHD_ST_RX2;		
		break;
		case 3:
			CHNTYPE = VHD_CORE_BP_RX3_TYPE; 
			CHNSTATUS = VHD_CORE_BP_RX3_STATUS;
			CLOCKDIV = VHD_SDI_BP_RX3_CLOCK_DIV;
			STRMTYPE = VHD_ST_RX3;		
		break;
		case 4:
			CHNTYPE = VHD_CORE_BP_RX4_TYPE; 
			CHNSTATUS = VHD_CORE_BP_RX4_STATUS;
			CLOCKDIV = VHD_SDI_BP_RX4_CLOCK_DIV;
			STRMTYPE = VHD_ST_RX4;		
		break;
		case 5:
			CHNTYPE = VHD_CORE_BP_RX5_TYPE; 
			CHNSTATUS = VHD_CORE_BP_RX5_STATUS;
			CLOCKDIV = VHD_SDI_BP_RX5_CLOCK_DIV;
			STRMTYPE = VHD_ST_RX5;		
		break;
		case 6:
			CHNTYPE = VHD_CORE_BP_RX6_TYPE; 
			CHNSTATUS = VHD_CORE_BP_RX6_STATUS;
			CLOCKDIV = VHD_SDI_BP_RX6_CLOCK_DIV;
			STRMTYPE = VHD_ST_RX6;		
		break;
		case 7:
			CHNTYPE = VHD_CORE_BP_RX7_TYPE; 
			CHNSTATUS = VHD_CORE_BP_RX7_STATUS;
			CLOCKDIV = VHD_SDI_BP_RX7_CLOCK_DIV;
			STRMTYPE = VHD_ST_RX7;		
		break;
		default:
		break;	
	}

	NbRxRequired = 1;
	NbTxRequired = 0;
	//TODO: For Error conditions in the else part we should log errors
	Result = VHD_GetApiInfo(&DllVersion,&NbBoards);
	if (Result == VHDERR_NOERROR) {
		if (NbBoards > 0) {
			if (SetNbChannels(BrdId, NbRxRequired, NbTxRequired)) {
				Result = VHD_OpenBoardHandle(BrdId, &ctx->BoardHandle, NULL, 0);
				if (Result == VHDERR_NOERROR) {
					VHD_GetBoardProperty(ctx->BoardHandle, CHNTYPE, &ctx->ChnType);
					if((ctx->ChnType == VHD_CHNTYPE_SDSDI) || (ctx->ChnType == VHD_CHNTYPE_HDSDI) || (ctx->ChnType == VHD_CHNTYPE_3GSDI)) {
						VHD_SetBoardProperty(ctx->BoardHandle, VHD_CORE_BP_BYPASS_RELAY_3, FALSE); // RELAY_3 or RELAY_0
						//VHD_SetBoardProperty(ctx->BoardHandle, VHD_CORE_BP_BYPASS_RELAY_0, TRUE);
						WaitForChannelLocked(ctx->BoardHandle, CHNSTATUS);    
						Result = VHD_GetBoardProperty(ctx->BoardHandle, CLOCKDIV, &ctx->ClockSystem);
						if(Result == VHDERR_NOERROR) {
                            Result = VHD_OpenStreamHandle(ctx->BoardHandle, STRMTYPE, VHD_SDI_STPROC_DISJOINED_VIDEO, NULL, &ctx->StreamHandle, NULL);
                            Result1 = VHD_OpenStreamHandle(ctx->BoardHandle, STRMTYPE, VHD_SDI_STPROC_DISJOINED_ANC, NULL, &ctx->StreamHandleANC, NULL);

							if (Result == VHDERR_NOERROR && Result1 == VHDERR_NOERROR) {
								Result = VHD_GetStreamProperty(ctx->StreamHandle, VHD_SDI_SP_VIDEO_STANDARD, &ctx->VideoStandard);
								if ((Result == VHDERR_NOERROR) && (ctx->VideoStandard != NB_VHD_VIDEOSTANDARDS)) {
									if (GetVideoCharacteristics(ctx->VideoStandard, &ctx->width, &ctx->height, &ctx->interlaced, &ctx->isHD)) {
										Result = VHD_GetStreamProperty(ctx->StreamHandle, VHD_SDI_SP_INTERFACE, &ctx->Interface);
										if((Result == VHDERR_NOERROR) && (ctx->Interface != NB_VHD_INTERFACE)) {
											VHD_SetStreamProperty(ctx->StreamHandle, VHD_SDI_SP_VIDEO_STANDARD, ctx->VideoStandard);
											VHD_SetStreamProperty(ctx->StreamHandle, VHD_CORE_SP_TRANSFER_SCHEME, VHD_TRANSFER_SLAVED);
											VHD_SetStreamProperty(ctx->StreamHandle, VHD_SDI_SP_INTERFACE, ctx->Interface);
											VHD_SetStreamProperty(ctx->StreamHandle,VHD_CORE_SP_BUFFERQUEUE_DEPTH,32); // AB
											VHD_SetStreamProperty(ctx->StreamHandle,VHD_CORE_SP_DELAY,1); // AB

                                            /* Start stream */
                                            Result = VHD_StartStream(ctx->StreamHandle);
                                            Result1 = VHD_StartStream(ctx->StreamHandleANC);
											if (Result == VHDERR_NOERROR && Result1 == VHDERR_NOERROR) {
                                                status = 0;
											} else {
												//log error
											}                                                         
										} else {
											//log error
										}
									} else {
										//log error
									}
								} else {
								
								}
							} else {
								//log error
							}
						} else {
							//log error
						}
						//VHD_SetBoardProperty(ctx->BoardHandle, VHD_CORE_BP_BYPASS_RELAY_0, TRUE);
					} else {
						//log error
					}
				} else {
					//log error
				}
			} else {
				//log error
			}
		} else {
			//log error
		}
	} else {
		//log error
	}
	return status;
}  


static int stop_video_stream(struct deltacast_ctx *ctx) {
	VHD_StopStream(ctx->StreamHandle);
    VHD_CloseStreamHandle(ctx->StreamHandle);
    VHD_StopStream(ctx->StreamHandleANC);
    VHD_CloseStreamHandle(ctx->StreamHandleANC);
	VHD_CloseBoardHandle(ctx->BoardHandle);

	return 0;
}

static int deltacast_read_header(AVFormatContext *avctx) {
    AVStream *v_stream;
    AVStream *a_stream;
    struct deltacast_ctx *ctx = (struct deltacast_ctx *) avctx->priv_data;

    // set AFD code to uninitialized value
    ctx->afd_ARCode = -1;

	int status =  start_video_stream(ctx);
	if (status == 0) {
        // create video stream
		v_stream = avformat_new_stream(avctx, NULL);
    	if (!v_stream) {
        	av_log(avctx, AV_LOG_ERROR, "Cannot add stream\n");
        	goto error;
		}
		v_stream->codecpar->codec_type  = AVMEDIA_TYPE_VIDEO;
		v_stream->codecpar->width       = ctx->width;
		v_stream->codecpar->height      = ctx->height;
		//v_stream->time_base.den      = ctx->bmd_tb_den;
		//v_stream->time_base.num      = ctx->bmd_tb_num;
        v_stream->avg_frame_rate.den  = 1000 + ctx->ClockSystem;
        v_stream->avg_frame_rate.num  = GetFPS(ctx->VideoStandard)*1000;
		//v_stream->codecpar->bit_rate    = av_image_get_buffer_size((AVPixelFormat)st->codecpar->format, ctx->bmd_width, ctx->bmd_height, 1) * 1/av_q2d(st->time_base) * 8;
		v_stream->codecpar->codec_id    = AV_CODEC_ID_RAWVIDEO;
		v_stream->codecpar->format      = AV_PIX_FMT_UYVY422;
		v_stream->codecpar->codec_tag   = MKTAG('U', 'Y', 'V', 'Y');
        ctx->video_st=v_stream;

        // create audio stream
        //a_stream = avformat_new_stream(avctx, NULL);
    }				
	
	return status;

error:	
	return AVERROR(EIO);
}

static int deltacast_read_close(AVFormatContext *avctx) {
	struct deltacast_ctx *ctx = (struct deltacast_ctx *) avctx->priv_data;
	int status = stop_video_stream(ctx);		
	return status;
}

static int read_afd_flag(struct deltacast_ctx* ctx) {
    VHD_AFD_AR_SLOT AfdArSlot;
    ULONG result;

    /* Set Afd line */
    memset(&AfdArSlot, 0, sizeof(VHD_AFD_AR_SLOT));
    AfdArSlot.LineNumber = 0;
    
    /* Try to lock next slot */
    result = VHD_LockSlotHandle(ctx->StreamHandleANC,&ctx->SlotHandle);
    if (result == VHDERR_NOERROR) 
    {
        /* Extract Afd Slot */
        result = VHD_SlotExtractAFD(ctx->SlotHandle, &AfdArSlot);
        if(result == VHDERR_NOERROR)
            ctx->afd_ARCode = AfdArSlot.AFD_ARCode;

        /* Unlock slot */ 
        VHD_UnlockSlotHandle(ctx->SlotHandle);
    }
    else if (result != VHDERR_TIMEOUT) {
        printf("ERROR : Cannot lock slot on RX0 stream. Result = 0x%08X (%s)\n",result, GetErrorDescription(result));
    }
    else
        printf("Timeout \n");
    
    return result;
}

static int deltacast_read_packet(AVFormatContext *avctx, AVPacket *pkt) {
 	ULONG result, bufferSize;
    BYTE  *pBuffer = NULL;
	struct deltacast_ctx *ctx = (struct deltacast_ctx *) avctx->priv_data;
    int err = 0;

    read_afd_flag(ctx);

    result = VHD_LockSlotHandle(ctx->StreamHandle, &ctx->SlotHandle);

	if (result == VHDERR_NOERROR) {
        result = VHD_GetSlotBuffer(ctx->SlotHandle, VHD_SDI_BT_VIDEO, &pBuffer,&bufferSize);

		//printf("Read Buffer Size = %d\n", bufferSize);

   		if (result == VHDERR_NOERROR) {
			err = av_packet_from_data(pkt, pBuffer, bufferSize);
			if (err) {
				//log error
			} else {
				//set flags in the packet
			}	
		} else {
			printf("\nERROR : Cannot get slot buffer. Result = 0x%08X (%s)\n", result, GetErrorDescription(result));
	   	}
		VHD_UnlockSlotHandle(ctx->SlotHandle); // AB
		VHD_GetStreamProperty(ctx->StreamHandle, VHD_CORE_SP_SLOTS_COUNT, &ctx->frameCount);
		VHD_GetStreamProperty(ctx->StreamHandle, VHD_CORE_SP_SLOTS_DROPPED, &ctx->dropped);
		pkt->pts = ctx->frameCount;
	} else if (result != VHDERR_TIMEOUT) {
		printf("\nERROR : Timeout. Result = 0x%08X (%s)\n", result, GetErrorDescription(result));
   		//cannot lock the stream
   		//log the error  
	} else {
   		result = VHDERR_TIMEOUT;
	}

	return result;
}

#define OFFSET(x) offsetof(struct deltacast_ctx, x)
#define DEC AV_OPT_FLAG_DECODING_PARAM

static const AVOption options[] = {
    { "v_channelIndex", "video channel index"  , OFFSET(v_channelIndex), AV_OPT_TYPE_INT   , { .i64 = 0   }, 0, 7, DEC },
    { NULL },
};

static const AVClass deltacast_demuxer_class = {
    .class_name = "Deltacast demuxer",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
    .category   = AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT,
};

AVInputFormat ff_deltacast_demuxer = {
    .name           = "deltacast",
    .long_name      = NULL_IF_CONFIG_SMALL("Deltacast input"),
    .flags          = AVFMT_NOFILE | AVFMT_RAWPICTURE,
    .priv_class     = &deltacast_demuxer_class,
    .priv_data_size = sizeof(struct deltacast_ctx),
    .read_header   = deltacast_read_header,
    .read_packet   = deltacast_read_packet,
    .read_close    = deltacast_read_close,
};

