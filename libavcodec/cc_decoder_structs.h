#ifndef CC_DECODER_STRUCTS_H
#define CC_DECODER_STRUCTS_H

#include "cc_common_timing.h"

#define MAXBFRAMES 50
#define SORTBUF (2*MAXBFRAMES+1)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cc_decode {

    int cc_stats[4];
    int saw_caption_block;
    int processed_enough; // If 1, we have enough lines, time, etc.

    int no_rollup; // If 1, write one line at a time
    int noscte20;
    int fix_padding; // Replace 0000 with 8080 in HDTV (needed for some cards)
   // enum ccx_output_format write_format; // 0=Raw, 1=srt, 2=SMI
    //struct ccx_boundary_time extraction_start, extraction_end; // Segment we actually process
    int64_t subs_delay; // ms to delay (or advance) subs
    int extract; // Extract 1st, 2nd or both fields
    int fullbin; // Disable pruning of padding cc blocks
    //struct cc_subtitle dec_sub;
    //enum ccx_bufferdata_type in_bufferdatatype;
   // unsigned int hauppauge_mode; // If 1, use PID=1003, process specially and so on

    int frames_since_last_gop;
    /* GOP-based timing */
    int saw_gop_header;
    /* Time info for timed-transcript */
    int max_gop_length; // (Maximum) length of a group of pictures
    int last_gop_length; // Length of the previous group of pictures
    unsigned total_pulldownfields;
    unsigned total_pulldownframes;
    int program_number;
    //struct list_head list;
    struct cc_common_timing_ctx *timing;
    //enum ccx_code_type codec;
    // Set to true if data is buffered
    int has_ccdata_buffered;
    int is_alloc;

    struct avc_ctx *avc_ctx;
    void *private_data;

    /* General video information */
    unsigned int current_hor_size;
    unsigned int current_vert_size;
    unsigned int current_aspect_ratio;
    unsigned int current_frame_rate; // Assume standard fps, 29.97

    /* Required in es_function.c */
    int no_bitstream_error;
    int saw_seqgoppic;
    int in_pic_data;

    unsigned int current_progressive_sequence;
    unsigned int current_pulldownfields ;

    int temporal_reference;
    enum cc_frame_type picture_coding_type;
    unsigned picture_structure;
    unsigned repeat_first_field;
    unsigned progressive_frame;
    unsigned pulldownfields;
    /* Required in es_function.c and es_userdata.c */
    unsigned top_field_first; // Needs to be global

    /* Stats. Modified in es_userdata.c*/
    int stat_numuserheaders;
    int stat_dvdccheaders;
    int stat_scte20ccheaders;
    int stat_replay5000headers;
    int stat_replay4000headers;
    int stat_dishheaders;
    int stat_hdtv;
    int stat_divicom;
    int false_pict_header;


    int current_field;
    // Analyse/use the picture information
    int maxtref; // Use to remember the temporal reference number

    int cc_data_count[SORTBUF];
    // Store fts;
    int64_t cc_fts[SORTBUF];
    // Store HD CC packets
    unsigned char cc_data_pkts[SORTBUF][10*31*3+1]; // *10, because MP4 seems to have different limits

    // The sequence number of the current anchor frame.  All currently read
    // B-Frames belong to this I- or P-frame.
    int anchor_seq_number;
    // struct ccaptionXDS_dec_context *xds_ctx;
    //struct ccx_decoder_vbi_ctx *vbi_decoder;

    //int (*writedata)(const unsigned char *data, int length, void *private_data, struct cc_subtitle *sub);
} cc_decode;


#ifdef __cplusplus
}
#endif

#endif /* CC_DECODER_STRUCTS_H */
