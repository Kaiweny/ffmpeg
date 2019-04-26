#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include "libavutil/opt.h"

#include "VideoMasterHD_Core.h"
#include "VideoMasterHD_Sdi.h"
#include "VideoMasterHD_Sdi_Vbi.h"
#include "VideoMasterHD_Sdi_VbiData.h"
#include "VideoMasterHD_Sdi_Audio.h"

#include "libavdevice/deltacast/Tools.h"

#define CLOCK_SYSTEM    VHD_CLOCKDIV_1001

const ULONG MOCK_TIME_BASE = 1000;

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
    ULONG audFrameCount;
    ULONG audDropped;

    AVStream *audio_st;
    void *AudioInfo;
    void **pAudioChn;
    ULONG channels;
    ULONG pairs;

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
    
    /* Closed Caption Buffer Info */
    BYTE  *cc_buffer;
    ULONG cc_buffer_size;
};

static int alloc_packet_side_data_from_buffer(AVPacket *packet, BYTE  *buffer, ULONG buffer_size) {
    // TODO Mitch: function to create side data with cc data in it
    uint8_t* dst_data = av_packet_new_side_data(packet, AV_PKT_DATA_A53_CC, buffer_size);
    if (!dst_data) {
        av_packet_free_side_data(packet);
        return AVERROR(ENOMEM);
    }
    memcpy(dst_data, buffer, buffer_size);
    return 0;
}

static int alloc_packet_from_buffer(AVPacket *packet, BYTE  *buffer, ULONG buffer_size) {
    packet->buf = av_buffer_alloc(buffer_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!packet->buf) {
        return AVERROR(ENOMEM);
    }
    memcpy(packet->buf->data, buffer, buffer_size + AV_INPUT_BUFFER_PADDING_SIZE);
    packet->data = packet->buf->data;
    packet->size = buffer_size;
    return 0;
}

static int start_stream(struct deltacast_ctx *ctx) {
    int status = 1;
    ULONG Result,DllVersion,NbBoards;
    ULONG BrdId = 0;
    ULONG NbRxRequired, NbTxRequired;

    VHD_CORE_BOARDPROPERTY CHNTYPE;
    VHD_CORE_BOARDPROPERTY CHNSTATUS;
    VHD_SDI_BOARDPROPERTY CLOCKDIV;
    VHD_STREAMTYPE STRMTYPE;

    VHD_VBILINE pCaptureLines[VHD_MAXNB_VBICAPTURELINE];

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
                		WaitForChannelLocked(ctx->BoardHandle, CHNSTATUS);    
                        Result = VHD_GetBoardProperty(ctx->BoardHandle, CLOCKDIV, &ctx->ClockSystem);
                		if(Result == VHDERR_NOERROR) {
                            Result = VHD_OpenStreamHandle(ctx->BoardHandle, STRMTYPE, VHD_SDI_STPROC_DISJOINED_VIDEO, NULL, &ctx->StreamHandle, NULL);
                            Result += VHD_OpenStreamHandle(ctx->BoardHandle, STRMTYPE, VHD_SDI_STPROC_DISJOINED_ANC, NULL, &ctx->StreamHandleANC, NULL);
                			if (Result == VHDERR_NOERROR) {
                                Result = VHD_GetStreamProperty(ctx->StreamHandle, VHD_SDI_SP_VIDEO_STANDARD, &ctx->VideoStandard);
                                Result += VHD_GetStreamProperty(ctx->StreamHandleANC, VHD_SDI_SP_VIDEO_STANDARD, &ctx->VideoStandard);
                				if ((Result == VHDERR_NOERROR) && (ctx->VideoStandard != NB_VHD_VIDEOSTANDARDS)) {
                					if (GetVideoCharacteristics(ctx->VideoStandard, &ctx->width, &ctx->height, &ctx->interlaced, &ctx->isHD)) {
                                        Result = VHD_GetStreamProperty(ctx->StreamHandle, VHD_SDI_SP_INTERFACE, &ctx->Interface);
                                        Result += VHD_GetStreamProperty(ctx->StreamHandleANC, VHD_SDI_SP_INTERFACE, &ctx->Interface);
                						if((Result == VHDERR_NOERROR) && (ctx->Interface != NB_VHD_INTERFACE)) {
                							VHD_SetStreamProperty(ctx->StreamHandle, VHD_SDI_SP_VIDEO_STANDARD, ctx->VideoStandard);
                							VHD_SetStreamProperty(ctx->StreamHandle, VHD_CORE_SP_TRANSFER_SCHEME, VHD_TRANSFER_SLAVED);
                                            VHD_SetStreamProperty(ctx->StreamHandle, VHD_SDI_SP_INTERFACE, ctx->Interface);

                                            VHD_SetStreamProperty(ctx->StreamHandleANC, VHD_SDI_SP_VIDEO_STANDARD, ctx->VideoStandard);
                							VHD_SetStreamProperty(ctx->StreamHandleANC, VHD_CORE_SP_TRANSFER_SCHEME, VHD_TRANSFER_SLAVED);
                                            VHD_SetStreamProperty(ctx->StreamHandleANC, VHD_SDI_SP_INTERFACE, ctx->Interface);

                							VHD_SetStreamProperty(ctx->StreamHandle,VHD_CORE_SP_BUFFERQUEUE_DEPTH,32); // AB
                                            VHD_SetStreamProperty(ctx->StreamHandle,VHD_CORE_SP_DELAY,1); // AB
                                            
                                            if (ctx->interlaced) {// Merge top and bottom fields (interleave lines) 
                								VHD_SetStreamProperty(ctx->StreamHandle,VHD_CORE_SP_FIELD_MERGE,TRUE); // AB
                                            }
                                            
                                            /* Set line to capture: While in extraction mode (VHD_SlotExtractClosedCaption), setting default 
                                               values of 0 will cause default line values to be used according to the current standard */
                                            memset(pCaptureLines,0,VHD_MAXNB_VBICAPTURELINE * sizeof(VHD_VBILINE));

                                            Result = VHD_VbiSetCaptureLines(ctx->StreamHandleANC, pCaptureLines);
                                            if (Result == VHDERR_NOERROR) {
                                                /* Start stream */
                                                Result = VHD_StartStream(ctx->StreamHandle);
                                                Result += VHD_StartStream(ctx->StreamHandleANC);
                                                if (Result == VHDERR_NOERROR) {
                                                    status = 0;
                                                } else {
                                                    //log error
                                                }
                                            } else {
                                                // log error
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

static int free_audio_data(struct deltacast_ctx *ctx) {
    // free audio buffers
    for (int pair = 0; pair < ctx->pairs; pair++) {
        free(((VHD_AUDIOCHANNEL**)ctx->pAudioChn)[pair]->pData);
        ((VHD_AUDIOCHANNEL**)ctx->pAudioChn)[pair]->pData = NULL;
    }

    // free audio channel pointers
    free((VHD_AUDIOCHANNEL**)ctx->pAudioChn);
    ctx->pAudioChn = NULL;

    // free audio info
    free((VHD_AUDIOINFO*)ctx->AudioInfo);
    ctx->AudioInfo = NULL;

    return 0;
}

static int GetFPS(ULONG VideoStandard) {
    int fps = 0;

    switch (VideoStandard)
    {
      case VHD_VIDEOSTD_S259M_NTSC: fps = 30; break;
      case VHD_VIDEOSTD_S259M_PAL: fps = 25; break;
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

static int deltacast_read_header(AVFormatContext *avctx) {
    struct deltacast_ctx *ctx = (struct deltacast_ctx *) avctx->priv_data;

    AVStream *v_stream;
    AVStream *a_stream;

    // Keep side data for closed captions embedding into video packet
    avctx->flags = AVFMT_FLAG_KEEP_SIDE_DATA;

    // set AFD code to uninitialized value
    ctx->afd_ARCode = -1;

    // set closed caption buffer info
    ctx->cc_buffer = malloc(MAX_CC_DATA_SIZE);
    ctx->cc_buffer_size = 0;

    int status =  start_stream(ctx);
    if (status == 0) {
        /* #### create video stream #### */
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
        if (ctx->interlaced) {
            v_stream->avg_frame_rate.num  = GetFPS(ctx->VideoStandard) * 1000 / 2;
        }
        else {
            v_stream->avg_frame_rate.num  = GetFPS(ctx->VideoStandard) * 1000;
        }
        av_log(avctx, AV_LOG_ERROR, "Video Standard %d (%d) %d/%d\n", ctx->VideoStandard, GetFPS(ctx->VideoStandard), v_stream->avg_frame_rate.num, v_stream->avg_frame_rate.den);
        //v_stream->codecpar->bit_rate    = av_image_get_buffer_size((AVPixelFormat)st->codecpar->format, ctx->bmd_width, ctx->bmd_height, 1) * 1/av_q2d(st->time_base) * 8;
        v_stream->codecpar->codec_id    = AV_CODEC_ID_RAWVIDEO;
        v_stream->codecpar->format      = AV_PIX_FMT_UYVY422;
        v_stream->codecpar->codec_tag   = MKTAG('U', 'Y', 'V', 'Y');
        // Choose a time base which will cancel out with frame rate. This results in a
        // PTS duration of MOCK_TIME_BASE.
        v_stream->time_base.num = v_stream->avg_frame_rate.den; // 1
        v_stream->time_base.den = MOCK_TIME_BASE * v_stream->avg_frame_rate.num; // 90000
        ctx->video_st=v_stream;

        /* #### create audio stream #### */
        a_stream = avformat_new_stream(avctx, NULL);
        if (!a_stream) {
            av_log(avctx, AV_LOG_ERROR, "Cannot add stream\n");
            goto error;
        }
        a_stream->codecpar->codec_type  = AVMEDIA_TYPE_AUDIO;
        a_stream->codecpar->codec_id    = AV_CODEC_ID_PCM_S16LE;
        a_stream->codecpar->format = AV_SAMPLE_FMT_S16;
        a_stream->codecpar->channel_layout = AV_CH_LAYOUT_STEREO;
        a_stream->codecpar->sample_rate = 48000;
        ctx->channels = 2; // TODO-Mitch: Hardcode temp., can be specified by user?
        a_stream->codecpar->channels    = ctx->channels;
        a_stream->time_base.num = v_stream->avg_frame_rate.den; // 1
        a_stream->time_base.den = MOCK_TIME_BASE * v_stream->avg_frame_rate.num; // 90000
        ctx->audio_st = a_stream;

        // initialize deltacast ctx VHD_AUDIOINFO struct
        VHD_AUDIOINFO *AudioInfo;
        AudioInfo = (VHD_AUDIOINFO*)malloc(sizeof(VHD_AUDIOINFO));

        // initialize deltacast ctx VHD_AUDIOCHANNEL array of pointers
        VHD_AUDIOCHANNEL **pAudioChn;
        if (ctx->channels <= 16) {
            ctx->pairs = (ctx->channels / 2) + (ctx->channels % 2); // stereo pair includes 2 channels
            pAudioChn = (VHD_AUDIOCHANNEL**)malloc(sizeof(VHD_AUDIOCHANNEL*) * ctx->pairs);
        } else {
            av_log(avctx, AV_LOG_ERROR, "ERROR : Invalid number of Audio Channels. %d Channels Requested > 16 Channel Limit\n", ctx->channels);
            status = VHDERR_BADARG;
        }

        // Configure Audio info: 48kHz - 16 bits audio reception on required channels
        memset(AudioInfo, 0, sizeof(VHD_AUDIOINFO));
        int grp = 0;
        int grp_pair = 0;
        for (int pair = 0; pair < ctx->pairs; pair++) {
            grp += ( (pair / 2) >= 1  &&  (pair % 2) == 0) ? 1 : 0;

            pAudioChn[pair]=&AudioInfo->pAudioGroups[grp].pAudioChannels[grp_pair*2];
            pAudioChn[pair]->Mode=AudioInfo->pAudioGroups[grp].pAudioChannels[grp_pair*2+1].Mode=VHD_AM_STEREO;
            pAudioChn[pair]->BufferFormat=AudioInfo->pAudioGroups[grp].pAudioChannels[grp_pair*2+1].BufferFormat=VHD_AF_16;
            grp_pair = !grp_pair;
        }

        // Give the deltacast ctx the allocated Audio info
        ctx->AudioInfo = AudioInfo;

        ULONG NbOfSamples, AudioBufferSize;
        /* Get the biggest audio frame size */
        NbOfSamples = VHD_GetNbSamples((VHD_VIDEOSTANDARD)ctx->VideoStandard, CLOCK_SYSTEM, VHD_ASR_48000, 0);
        AudioBufferSize = NbOfSamples*VHD_GetBlockSize(pAudioChn[0]->BufferFormat, pAudioChn[0]->Mode);
        
        /* Create audio buffer */
        for (int pair = 0; pair < ctx->pairs; pair++) {
            pAudioChn[pair]->pData = (BYTE*)malloc(sizeof(BYTE) * AudioBufferSize);
        }

        /* Set the audio buffer size */
        for (int pair = 0; pair < ctx->pairs; pair++) {
            pAudioChn[pair]->DataSize = AudioBufferSize;
        }

        // Give the deltacast ctx the allocated audio channel pointers
        ctx->pAudioChn = pAudioChn;
    }

    return status;

error:    
    return AVERROR(EIO);
}

static int deltacast_read_close(AVFormatContext *avctx) {
    struct deltacast_ctx *ctx = (struct deltacast_ctx *) avctx->priv_data;
    int status = stop_video_stream(ctx);
    free_audio_data(ctx);
    return status;
}

#define CC_LINE1 21
#define CC_LINE2 284

static int read_cc_data(AVFormatContext *avctx, struct deltacast_ctx* ctx) {
    ULONG result;
    VHD_CC_SLOT CCSlot;

    /* Set CC line */
    memset(&CCSlot, 0, sizeof(VHD_CC_SLOT));
    if (ctx->VideoStandard == VHD_VIDEOSTD_S259M_NTSC) {
        CCSlot.CCInfo.CCType = VHD_CC_EIA_608B;
        CCSlot.CCInfo.pLineNumber[0] = CC_LINE1;
        CCSlot.CCInfo.pLineNumber[1] = CC_LINE2;
    }
    else {
        CCSlot.CCInfo.CCType = VHD_CC_EIA_708;
    }

    /* Extract WSS Slot */
    result = VHD_SlotExtractClosedCaptions(ctx->SlotHandle, &CCSlot);
    if (result == VHDERR_NOERROR) {
        memcpy(ctx->cc_buffer, CCSlot.CCData.pData, CCSlot.CCData.DataSize);
        ctx->cc_buffer_size = CCSlot.CCData.DataSize;
    }
    else {
        av_log(avctx, AV_LOG_ERROR, "ERROR : Cannot extract closed captions. Result = 0x%08X (%s)\n",result, GetErrorDescription(result));
    }

    return result;
}

static int read_video_data(AVFormatContext *avctx, struct deltacast_ctx* ctx, AVPacket *pkt) {
    ULONG result;
    ULONG bufferSize;
    BYTE  *pBuffer = NULL;
    int err = 0;

    // Fill packet data with video data
    result = VHD_LockSlotHandle(ctx->StreamHandle, &ctx->SlotHandle);
    if (result == VHDERR_NOERROR) {
        result = VHD_GetSlotBuffer(ctx->SlotHandle, VHD_SDI_BT_VIDEO, &pBuffer,&bufferSize);

           if (result == VHDERR_NOERROR) {
            /*err = av_packet_from_data(pkt, pBuffer, bufferSize);
            if (err) {
                //log error
            } else {
                //set flags in the packet
            }*/

            err = alloc_packet_from_buffer(pkt, pBuffer, bufferSize);
            if (err) {
                //log error
            } else {
                //set flags in the packet
            }
        } else {
            av_log(avctx, AV_LOG_ERROR, "ERROR : Cannot get slot buffer. Result = 0x%08X (%s)\n", result, GetErrorDescription(result));
        }
        VHD_UnlockSlotHandle(ctx->SlotHandle); // AB
        VHD_GetStreamProperty(ctx->StreamHandle, VHD_CORE_SP_SLOTS_COUNT, &ctx->frameCount);
        VHD_GetStreamProperty(ctx->StreamHandle, VHD_CORE_SP_SLOTS_DROPPED, &ctx->dropped);
        // Since we control the creation of the PTS, we multiply the frame count by an
        // arbitrary number MOCK_TIME_BASE. This allows for PTS values of whole integers.
        // note: MOCK_TIME_BASE is made large to ensure good precision handling for
        // ffmpeg's frame->pkt_duration calculation.
        pkt->pts = ctx->frameCount * MOCK_TIME_BASE;
    } else if (result != VHDERR_TIMEOUT) {
        av_log(avctx, AV_LOG_ERROR, "ERROR : Timeout. Result = 0x%08X (%s)\n", result, GetErrorDescription(result));
    } else {
        result = VHDERR_TIMEOUT;
    }

    // Fill video packet side data with Closed Captions data
    if (result == VHDERR_NOERROR) {
        if (ctx->cc_buffer_size > 0) {
            err = alloc_packet_side_data_from_buffer(pkt, ctx->cc_buffer, ctx->cc_buffer_size);
            memset(ctx->cc_buffer, 0, MAX_CC_DATA_SIZE);
            ctx->cc_buffer_size = 0;
        }
        if (err) {
            av_log(avctx, AV_LOG_ERROR, "Error on create cc side data\n");
            result = VHDERR_OPERATIONFAILED;
        } else {
            //set flags in the packet
        }
    }

    return result;
}

static int read_afd_flag(struct deltacast_ctx* ctx) {
    VHD_AFD_AR_SLOT AfdArSlot;
    ULONG result;

    /* Set Afd line */
    memset(&AfdArSlot, 0, sizeof(VHD_AFD_AR_SLOT));
    AfdArSlot.LineNumber = 0;

    /* Extract Afd Slot */
    result = VHD_SlotExtractAFD(ctx->SlotHandle, &AfdArSlot);
    if(result == VHDERR_NOERROR)
        ctx->afd_ARCode = AfdArSlot.AFD_ARCode;

    return result;
}

static int read_audio_data(AVFormatContext *avctx, struct deltacast_ctx* ctx, AVPacket *pkt) {
    ULONG result;
    ULONG AudioBufferSize;
    int err = 0;

    /* Try to lock next slot */
    result = VHD_LockSlotHandle(ctx->StreamHandleANC, &ctx->SlotHandle);
    if (result == VHDERR_NOERROR) {
        // Keep a copy of the max audio buffer size
        AudioBufferSize = ((VHD_AUDIOCHANNEL**)ctx->pAudioChn)[0]->DataSize;

        /* Extract AFD metadata */
        read_afd_flag(ctx);

        /* Extract CC data */
        read_cc_data(avctx, ctx);

        /* Extract Audio */
        result = VHD_SlotExtractAudio(ctx->SlotHandle, (VHD_AUDIOINFO*)ctx->AudioInfo);
        if(result==VHDERR_NOERROR) {
            // TODO-Mitch: only read first stereo pair for now
            err = alloc_packet_from_buffer(pkt,
                                           ((VHD_AUDIOCHANNEL**)ctx->pAudioChn)[0]->pData,
                                           ((VHD_AUDIOCHANNEL**)ctx->pAudioChn)[0]->DataSize);
            if (err) {
                //log error
            } else {
                //set flags in the packet
            }

        } else {
            av_log(avctx, AV_LOG_ERROR, "ERROR!:: Unable to Extract Audio from slot!, Result = 0x%08X\n", result);
        }

        /* Unlock slot */
        VHD_UnlockSlotHandle(ctx->SlotHandle);

        /* Get some statistics */
        VHD_GetStreamProperty(ctx->StreamHandleANC, VHD_CORE_SP_SLOTS_COUNT, &ctx->audFrameCount);
        VHD_GetStreamProperty(ctx->StreamHandleANC, VHD_CORE_SP_SLOTS_DROPPED, &ctx->audDropped);
        //pkt->pts = ctx->audFrameCount;
        // See read_video_data(..) for details regarding PTS calculations.
        pkt->pts = ctx->audFrameCount * MOCK_TIME_BASE;
        // reset channel to max audio buffer size
        ((VHD_AUDIOCHANNEL**)ctx->pAudioChn)[0]->DataSize = AudioBufferSize;
    }
    else if (result != VHDERR_TIMEOUT) {
        av_log(avctx, AV_LOG_ERROR, "ERROR : Cannot lock slot on RX0 stream. Result = 0x%08X (%s)\n",result, GetErrorDescription(result));
    }
    else {
        av_log(avctx, AV_LOG_ERROR, "ERROR : Timeout");
    }

    return result;
}

static int deltacast_read_packet(AVFormatContext *avctx, AVPacket *pkt) {
    ULONG result;
    struct deltacast_ctx *ctx = (struct deltacast_ctx *) avctx->priv_data;

    // choose to read video or audio data depending on the current pts of each media type
    // i.e. make video packets have higher priority than audio packets for the current
    // pts value.
    // Reading audio frames first as it contains the CC data for the next video frame
    if (ctx->audFrameCount <= ctx->frameCount) {
        pkt->stream_index = ctx->audio_st->index;
        result = read_audio_data(avctx, ctx, pkt);
    }
    else {
        pkt->stream_index = ctx->video_st->index;
        result = read_video_data(avctx, ctx, pkt);
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
    .flags          = AVFMT_NOFILE,
    .priv_class     = &deltacast_demuxer_class,
    .priv_data_size = sizeof(struct deltacast_ctx),
    .read_header   = deltacast_read_header,
    .read_packet   = deltacast_read_packet,
    .read_close    = deltacast_read_close,
};

