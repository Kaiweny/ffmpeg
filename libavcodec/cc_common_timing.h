#ifndef CC_COMMON_TIMING_H
#define CC_COMMON_TIMING_H

#include "cc_common_timing.h"
#include "avcodec.h"
#ifdef __cplusplus
extern "C" {
#endif

 enum cc_frame_type {
    CC_FRAME_TYPE_RESET_OR_UNKNOWN = 0,
    CC_FRAME_TYPE_I_FRAME = 1,
    CC_FRAME_TYPE_P_FRAME = 2,
    CC_FRAME_TYPE_B_FRAME = 3,
};

typedef struct cc_common_timing_ctx {
	int pts_set; //0 = No, 1 = received, 2 = min_pts set
	int64_t current_pts;
	enum cc_frame_type current_picture_coding_type;
	int current_tref; // Store temporal reference of current frame
	int64_t min_pts;
	int64_t max_pts;
	int64_t sync_pts;
	int64_t minimum_fts; // No screen should start before this FTS
	int64_t fts_now; // Time stamp of current file (w/ fts_offset, w/o fts_global)
	int64_t fts_offset; // Time before first sync_pts
	int64_t fts_fc_offset; // Time before first GOP
	int64_t fts_max; // Remember the maximum fts that we saw in current file
	int64_t fts_global; // Duration of previous files (-ve mode)
	int sync_pts2fts_set; //0 = No, 1 = Yes
	int64_t sync_pts2fts_fts;
	int64_t sync_pts2fts_pts;
} cc_common_timing_ctx;

#define CC_OK       0
#define CC_EINVAL  -102
int64_t get_fts(struct cc_common_timing_ctx *ctx, int current_field);
void set_current_pts(struct cc_common_timing_ctx *ctx, int64_t pts);
int set_fts(struct cc_common_timing_ctx *ctx, AVRational current_fps);

void dinit_timing_ctx(struct cc_common_timing_ctx **arg);
struct cc_common_timing_ctx *init_timing_ctx(void);
void freep(void *arg);
#ifdef __cplusplus
}
#endif

#endif /* CC_COMMON_TIMING_H */
