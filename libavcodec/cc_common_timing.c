#include "cc_common_timing.h"

void freep(void *arg) {
	void **ptr = (void **)arg;
	if (*ptr)
		free(*ptr);
	*ptr = NULL;

}

void dinit_timing_ctx(struct cc_common_timing_ctx **arg) {
	freep(arg);
}

cc_common_timing_ctx *init_timing_ctx() {
	struct cc_common_timing_ctx *ctx = malloc(sizeof(struct cc_common_timing_ctx));
	if(!ctx)
		return NULL;

	ctx->pts_set = 0;
	ctx->current_tref = 0;
	ctx->current_pts = 0;
	ctx->current_picture_coding_type = CC_FRAME_TYPE_RESET_OR_UNKNOWN;
	ctx->min_pts = 0x01FFFFFFFFLL; // 33 bit
	ctx->max_pts = 0;
	ctx->sync_pts = 0;
	ctx->minimum_fts = 0;
	ctx->sync_pts2fts_set = 0;
	ctx->sync_pts2fts_fts = 0;
	ctx->sync_pts2fts_pts = 0;

	ctx->fts_now = 0; // Time stamp of current file (w/ fts_offset, w/o fts_global)
	ctx->fts_offset = 0; // Time before first sync_pts
	ctx->fts_fc_offset = 0; // Time before first GOP
	ctx->fts_max = 0; // Remember the maximum fts that we saw in current file
	ctx->fts_global = 0; // Duration of previous files (-ve mode), see c1global

	return ctx;
}

int64_t get_fts(struct cc_common_timing_ctx *ctx, int current_field) {
    return 0;
}

void set_current_pts(struct cc_common_timing_ctx *ctx, int64_t pts) {
    ctx->current_pts = pts;
    if(ctx->pts_set == 0)
        ctx->pts_set = 1;
}

int MPEG_CLOCK_FREQ = 90000; // This "constant" is part of the standard
int set_fts(struct cc_common_timing_ctx *ctx, AVRational current_fps) {
    int pts_jump = 0;
    unsigned total_frames_count = 0; //is being incremented in slice_header in ccextractor
    //have to find a place to stash it such that it can be incremented or passed as an argument

    if (!ctx->pts_set)
        return CC_OK;

    // First check for timeline jump (only when min_pts was set (implies sync_pts)).
    int dif = 0;
    if (ctx->pts_set == 2) {
        dif= (int)(ctx->current_pts - ctx->sync_pts)/MPEG_CLOCK_FREQ;
        if (dif < -0.2 || dif >= 5) {
            pts_jump = 1;
            // Discard the gap if it is not on an I-frame or temporal reference zero.
            if(ctx->current_tref != 0 && ctx->current_picture_coding_type != CC_FRAME_TYPE_I_FRAME) {
                ctx->fts_now = ctx->fts_max;
		return CC_OK;
            }
	}
    }

    // Set min_pts, fts_offset
    if (ctx->pts_set != 0) {
        ctx->pts_set = 2;

	// Use this part only the first time min_pts is set. Later treat
	// it as a reference clock change
	if (ctx->current_pts < ctx->min_pts && !pts_jump) {
            // If this is the first GOP, and seq 0 was not encountered yet
            // we might reset min_pts/fts_offset again
            ctx->min_pts = ctx->current_pts;
            // Avoid next async test
            ctx->sync_pts = (int64_t)(ctx->current_pts
                - ctx->current_tref * 1000.0 * current_fps.den / current_fps.num * (MPEG_CLOCK_FREQ / 1000));

            if(ctx->current_tref == 0) {   // Earliest time in GOP.
                ctx->fts_offset = 0;
            } else {   // It needs to be "+1" because the current frame is
				// not yet counted.
		ctx->fts_offset = (int64_t)((total_frames_count + 1) * 1000.0 * current_fps.den / current_fps.num);
	    }
	}

	// Set sync_pts, fts_offset
	if(ctx->current_tref == 0)
            ctx->sync_pts = ctx->current_pts;

	if (ctx->pts_set) {
            // If pts_set is TRUE we have min_pts
            ctx->fts_now = (int64_t)((ctx->current_pts -
                ctx->min_pts) / (MPEG_CLOCK_FREQ / 1000) + ctx->fts_offset);

            if (!ctx->sync_pts2fts_set) {
                ctx->sync_pts2fts_pts = ctx->current_pts;
		ctx->sync_pts2fts_fts = ctx->fts_now;
		ctx->sync_pts2fts_set = 1;
            }
	}
    }

    if (ctx->fts_now > ctx->fts_max) {
        ctx->fts_max = ctx->fts_now;
    }
    return CC_OK;
}