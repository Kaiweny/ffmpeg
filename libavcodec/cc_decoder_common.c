#include "cc_decoder_common.h"
#include "cc_common_timing.h"
//#include "cc_sequencing.h"

void dinit_cc_decode(struct cc_decode **ctx) {
    struct cc_decode *lctx = *ctx;
    dinit_timing_ctx(&lctx->timing);
    freep(ctx);
}

cc_decode* init_cc_decode() {
    cc_decode *ctx = NULL;

    ctx = malloc(sizeof(cc_decode));
    if(!ctx)
        return NULL;

    ctx->timing = init_timing_ctx();


    ctx->current_field = 1;
    //ctx->private_data = setting->private_data;
    //ctx->fix_padding = setting->fix_padding;
    //ctx->write_format =  setting->output_format;
    //ctx->subs_delay =  setting->subs_delay;
    //ctx->extract = setting->extract;
    //ctx->fullbin = setting->fullbin;
    //ctx->hauppauge_mode = setting->hauppauge_mode;
    //ctx->program_number = setting->program_number;
    ctx->processed_enough = 0;
    ctx->max_gop_length = 0;
    ctx->has_ccdata_buffered = 0;
    //ctx->in_bufferdatatype = CCX_UNKNOWN;
    ctx->frames_since_last_gop  = 0;
    ctx->total_pulldownfields   = 0;
    ctx->total_pulldownframes   = 0;
    ctx->stat_numuserheaders    = 0;
    ctx->stat_dvdccheaders      = 0;
    ctx->stat_scte20ccheaders   = 0;
    ctx->stat_replay5000headers = 0;
    ctx->stat_replay4000headers = 0;
    ctx->stat_dishheaders       = 0;
    ctx->stat_hdtv              = 0;
    ctx->stat_divicom           = 0;
    ctx->false_pict_header = 0;

	//memcpy(&ctx->extraction_start, &setting->extraction_start,sizeof(struct ccx_boundary_time));
	//memcpy(&ctx->extraction_end, &setting->extraction_end,sizeof(struct ccx_boundary_time));
#if 0
	if (setting->send_to_srv)
		ctx->writedata = net_send_cc;
	else if (setting->output_format==CCX_OF_RAW ||
		setting->output_format==CCX_OF_DVDRAW ||
		setting->output_format==CCX_OF_RCWT )
		ctx->writedata = writeraw;
	else if (setting->output_format==CCX_OF_SMPTETT ||
		setting->output_format==CCX_OF_SAMI ||
		setting->output_format==CCX_OF_SRT ||
		setting->output_format==CCX_OF_SSA ||
		setting->output_format == CCX_OF_WEBVTT ||
		setting->output_format==CCX_OF_TRANSCRIPT ||
		setting->output_format==CCX_OF_SPUPNG ||
		setting->output_format==CCX_OF_SIMPLE_XML ||
		setting->output_format==CCX_OF_G608 ||
		setting->output_format==CCX_OF_NULL ||
		setting->output_format==CCX_OF_CURL)
		ctx->writedata = process608;
	else
		fatal(CCX_COMMON_EXIT_BUG_BUG, "Invalid Write Format Selected");
#endif
	//memset (&ctx->dec_sub, 0,sizeof(ctx->dec_sub));

    // Initialize HDTV caption buffer
    //init_hdcc(ctx);

    ctx->current_hor_size = 0;
    ctx->current_vert_size = 0;
    ctx->current_aspect_ratio = 0;
    ctx->current_frame_rate = 4; // Assume standard fps, 29.97

    //Variables used while parsing elementry stream
    ctx->no_bitstream_error = 0;
    ctx->saw_seqgoppic = 0;
    ctx->in_pic_data = 0;

    ctx->current_progressive_sequence = 2;
    ctx->current_pulldownfields = 32768;

    ctx->temporal_reference = 0;
    ctx->maxtref = 0;
    ctx->picture_coding_type = CC_FRAME_TYPE_RESET_OR_UNKNOWN;
    ctx->picture_structure = 0;
    ctx->top_field_first = 0;
    ctx->repeat_first_field = 0;
    ctx->progressive_frame = 0;
    ctx->pulldownfields = 0;
    //es parser related variable ends here

    memset(ctx->cc_stats, 0, 4 * sizeof(int));

    ctx->anchor_seq_number = -1;
    // Init XDS buffers

    //ctx->vbi_decoder = NULL;
    return ctx;
}

/**
void flush_cc_decode(struct lib_cc_decode *ctx, struct cc_subtitle *sub)
{
	if(ctx->codec == CCX_CODEC_ATSC_CC)
	{
		if (ctx->extract != 2)
		{
			if (ctx->write_format==CCX_OF_SMPTETT || ctx->write_format==CCX_OF_SAMI ||
					ctx->write_format==CCX_OF_SRT || ctx->write_format==CCX_OF_TRANSCRIPT ||
					ctx->write_format == CCX_OF_WEBVTT || ctx->write_format == CCX_OF_SPUPNG ||
					ctx->write_format == CCX_OF_SSA)
			{
				flush_608_context(ctx->context_cc608_field_1, sub);
			}
			else if(ctx->write_format == CCX_OF_RCWT)
			{
				// Write last header and data
				writercwtdata (ctx, NULL, sub);
			}
		}
		if (ctx->extract != 1)
		{
			if (ctx->write_format == CCX_OF_SMPTETT || ctx->write_format == CCX_OF_SAMI ||
					ctx->write_format == CCX_OF_SRT || ctx->write_format == CCX_OF_TRANSCRIPT ||
					ctx->write_format == CCX_OF_WEBVTT || ctx->write_format == CCX_OF_SPUPNG ||
					ctx->write_format == CCX_OF_SSA)
			{
				flush_608_context(ctx->context_cc608_field_2, sub);
			}
		}
	}
	if (ctx->dtvcc->is_active)
	{
		for (int i = 0; i < CCX_DTVCC_MAX_SERVICES; i++)
		{
			ccx_dtvcc_service_decoder *decoder = &ctx->dtvcc->decoders[i];
			if (!ctx->dtvcc->services_active[i])
				continue;
			if (decoder->cc_count > 0)
			{
				ctx->current_field = 3;
				ccx_dtvcc_decoder_flush(ctx->dtvcc, decoder);
			}
		}
	}
}
 */
