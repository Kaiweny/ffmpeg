#ifndef AVCODEC_CCAPTION708_DEC_H
#define AVCODEC_CCAPTION708_DEC_H

#include "libavutil/rational.h"
#include "cc_decoder_structs.h"
#include "libavutil/frame.h"

#define CCX_708_MAX_PACKET_LENGTH 128 //According to EIA-708B, part 5
#define CC_708_MAX_SERVICES 63

#define CCX_708_MAX_ROWS 16

#define CCX_708_MAX_COLUMNS (32*2)

#define CCX_708_SCREENGRID_ROWS 75
#define CCX_708_SCREENGRID_COLUMNS 210

#define CC_708_MAX_WINDOWS 8

#define CCX_708_FILENAME_TEMPLATE ".p%u.svc%02u"

#define CCX_708_NO_LAST_SEQUENCE -1


#define CCX_708_MUSICAL_NOTE_CHAR 9836 // Unicode Character 'BEAMED SIXTEENTH NOTES'

enum COMMANDS_C0_CODES
{
    C0_NUL = 	0x00,
    C0_ETX = 	0x03,
    C0_BS = 	0x08,
    C0_FF = 	0x0c,
    C0_CR = 	0x0d,
    C0_HCR = 	0x0e,
    C0_EXT1 = 0x10,
    C0_P16 = 	0x18
};

enum COMMANDS_C1_CODES
{
    C1_CW0 = 0x80,
    C1_CW1 = 0x81,
    C1_CW2 = 0x82,
    C1_CW3 = 0x83,
    C1_CW4 = 0x84,
    C1_CW5 = 0x85,
    C1_CW6 = 0x86,
    C1_CW7 = 0x87,
    C1_CLW = 0x88,
    C1_DSW = 0x89,
    C1_HDW = 0x8A,
    C1_TGW = 0x8B,
    C1_DLW = 0x8C,
    C1_DLY = 0x8D,
    C1_DLC = 0x8E,
    C1_RST = 0x8F,
    C1_SPA = 0x90,
    C1_SPC = 0x91,
    C1_SPL = 0x92,
    C1_RSV93 = 0x93,
    C1_RSV94 = 0x94,
    C1_RSV95 = 0x95,
    C1_RSV96 = 0x96,
    C1_SWA = 0x97,
    C1_DF0 = 0x98,
    C1_DF1 = 0x99,
    C1_DF2 = 0x9A,
    C1_DF3 = 0x9B,
    C1_DF4 = 0x9C,
    C1_DF5 = 0x9D,
    C1_DF6 = 0x9E,
    C1_DF7 = 0x9F
};

struct S_COMMANDS_C1
{
    int code;
    const char *name;
    const char *description;
    int length;
};

enum cc_708_window_justify
{
    WINDOW_JUSTIFY_LEFT	= 0,
    WINDOW_JUSTIFY_RIGHT	= 1,
    WINDOW_JUSTIFY_CENTER	= 2,
    WINDOW_JUSTIFY_FULL	= 3
};

enum cc_708_window_pd //Print Direction
{
    WINDOW_PD_LEFT_RIGHT = 0, //left -> right
    WINDOW_PD_RIGHT_LEFT = 1,
    WINDOW_PD_TOP_BOTTOM = 2,
    WINDOW_PD_BOTTOM_TOP = 3
};

enum cc_708_window_sd //Scroll Direction
{
    WINDOW_SD_LEFT_RIGHT = 0,
    WINDOW_SD_RIGHT_LEFT = 1,
    WINDOW_SD_TOP_BOTTOM = 2,
    WINDOW_SD_BOTTOM_TOP = 3
};

enum cc_708_window_sde //Scroll Display Effect
{
    WINDOW_SDE_SNAP = 0,
    WINDOW_SDE_FADE = 1,
    WINDOW_SDE_WIPE = 2
};

enum cc_708_window_ed //Effect Direction
{
    WINDOW_ED_LEFT_RIGHT = 0,
    WINDOW_ED_RIGHT_LEFT = 1,
    WINDOW_ED_TOP_BOTTOM = 2,
    WINDOW_ED_BOTTOM_TOP = 3
};

enum cc_708_window_fo //Fill Opacity
{
    WINDOW_FO_SOLID		= 0,
    WINDOW_FO_FLASH		= 1,
    WINDOW_FO_TRANSLUCENT	= 2,
    WINDOW_FO_TRANSPARENT = 3
};

enum cc_708_window_border
{
    WINDOW_BORDER_NONE			= 0,
    WINDOW_BORDER_RAISED			= 1,
    WINDOW_BORDER_DEPRESSED		= 2,
    WINDOW_BORDER_UNIFORM			= 3,
    WINDOW_BORDER_SHADOW_LEFT		= 4,
    WINDOW_BORDER_SHADOW_RIGHT	= 5
};

enum cc_708_pen_size
{
    PEN_SIZE_SMALL 	= 0,
    PEN_SIZE_STANDART = 1,
    PEN_SIZE_LARGE	= 2
};

enum cc_708_pen_font_style
{
    PEN_FONT_STYLE_DEFAULT_OR_UNDEFINED					= 0,
    PEN_FONT_STYLE_MONOSPACED_WITH_SERIFS					= 1,
    PEN_FONT_STYLE_PROPORTIONALLY_SPACED_WITH_SERIFS		= 2,
    PEN_FONT_STYLE_MONOSPACED_WITHOUT_SERIFS				= 3,
    PEN_FONT_STYLE_PROPORTIONALLY_SPACED_WITHOUT_SERIFS	= 4,
    PEN_FONT_STYLE_CASUAL_FONT_TYPE						= 5,
    PEN_FONT_STYLE_CURSIVE_FONT_TYPE						= 6,
    PEN_FONT_STYLE_SMALL_CAPITALS							= 7
};

enum cc_708_pen_text_tag
{
    PEN_TEXT_TAG_DIALOG						= 0,
    PEN_TEXT_TAG_SOURCE_OR_SPEAKER_ID			= 1,
    PEN_TEXT_TAG_ELECTRONIC_VOICE				= 2,
    PEN_TEXT_TAG_FOREIGN_LANGUAGE				= 3,
    PEN_TEXT_TAG_VOICEOVER					= 4,
    PEN_TEXT_TAG_AUDIBLE_TRANSLATION			= 5,
    PEN_TEXT_TAG_SUBTITLE_TRANSLATION			= 6,
    PEN_TEXT_TAG_VOICE_QUALITY_DESCRIPTION	= 7,
    PEN_TEXT_TAG_SONG_LYRICS					= 8,
    PEN_TEXT_TAG_SOUND_EFFECT_DESCRIPTION		= 9,
    PEN_TEXT_TAG_MUSICAL_SCORE_DESCRIPTION	= 10,
    PEN_TEXT_TAG_EXPLETIVE					= 11,
    PEN_TEXT_TAG_UNDEFINED_12					= 12,
    PEN_TEXT_TAG_UNDEFINED_13					= 13,
    PEN_TEXT_TAG_UNDEFINED_14					= 14,
    PEN_TEXT_TAG_NOT_TO_BE_DISPLAYED			= 15
};

enum cc_708_pen_offset
{
    PEN_OFFSET_SUBSCRIPT		= 0,
    PEN_OFFSET_NORMAL			= 1,
    PEN_OFFSET_SUPERSCRIPT	= 2
};

enum cc_708_pen_edge
{
    PEN_EDGE_NONE					= 0,
    PEN_EDGE_RAISED				= 1,
    PEN_EDGE_DEPRESSED			= 2,
    PEN_EDGE_UNIFORM				= 3,
    PEN_EDGE_LEFT_DROP_SHADOW		= 4,
    PEN_EDGE_RIGHT_DROP_SHADOW	= 5
};

enum cc_708_pen_anchor_point
{
    ANCHOR_POINT_TOP_LEFT 		= 0,
    ANCHOR_POINT_TOP_CENTER 		= 1,
    ANCHOR_POINT_TOP_RIGHT 		= 2,
    ANCHOR_POINT_MIDDLE_LEFT 		= 3,
    ANCHOR_POINT_MIDDLE_CENTER 	= 4,
    ANCHOR_POINT_MIDDLE_RIGHT 	= 5,
    ANCHOR_POINT_BOTTOM_LEFT 		= 6,
    ANCHOR_POINT_BOTTOM_CENTER 	= 7,
    ANCHOR_POINT_BOTTOM_RIGHT 	= 8
};

typedef struct cc_708_pen_color {
    int fg_color;
    int fg_opacity;
    int bg_color;
    int bg_opacity;
    int edge_color;
} cc_708_pen_color;

typedef struct cc_708_pen_attribs {
    int pen_size;
    int offset;
    int text_tag;
    int font_tag;
    int edge_type;
    int underline;
    int italic;
} cc_708_pen_attribs;

typedef struct cc_708_window_attribs {
    int justify;
    int print_direction;
    int scroll_direction;
    int word_wrap;
    int display_effect;
    int effect_direction;
    int effect_speed;
    int fill_color;
    int fill_opacity;
    int border_type;
    int border_color;
} cc_708_window_attribs;

/**
 * Since 1-byte and 2-byte symbols could appear in captions and
 * since we have to keep symbols alignment and several windows could appear on a screen at one time,
 * we use special structure for holding symbols
 */
typedef struct cc_708_symbol {
    unsigned short sym; //symbol itself, at least 16 bit
    unsigned char init; //initialized or not. could be 0 or 1
} cc_708_symbol;

#define SYM_SET(x, c) {x.init = 1; x.sym = c;}
#define SYM_SET_16(x, c1, c2) {x.init = 1; x.sym = (c1 << 8) | c2;}
#define SYM(x) ((unsigned char)(x.sym))
#define SYM_IS_EMPTY(x) (x.init == 0)
#define SYM_IS_SET(x) (x.init == 1)

typedef struct cc_708_window {
    int is_defined;
    int number;
    int priority;
    int col_lock;
    int row_lock;
    int visible;
    int anchor_vertical;
    int relative_pos;
    int anchor_horizontal;
    int row_count;
    int anchor_point;
    int col_count;
    int pen_style;
    int win_style;
    unsigned char commands[6]; // Commands used to create this window
    cc_708_window_attribs attribs;
    int pen_row;
    int pen_column;
    cc_708_symbol *rows[CCX_708_MAX_ROWS];
    cc_708_pen_color pen_colors[CCX_708_MAX_ROWS][CCX_708_SCREENGRID_COLUMNS];
    cc_708_pen_attribs pen_attribs[CCX_708_MAX_ROWS][CCX_708_SCREENGRID_COLUMNS];
    cc_708_pen_color pen_color_pattern;
    cc_708_pen_attribs pen_attribs_pattern;
    int memory_reserved;
    int is_empty;
    int64_t time_ms_show;
    int64_t time_ms_hide;
} cc_708_window;

/*
typedef struct tv_screen {
    cc_708_symbol chars[CCX_708_SCREENGRID_ROWS][CCX_708_SCREENGRID_COLUMNS];
    cc_708_pen_color pen_colors[CCX_708_SCREENGRID_ROWS][CCX_708_SCREENGRID_COLUMNS];
    cc_708_pen_attribs pen_attribs[CCX_708_SCREENGRID_ROWS][CCX_708_SCREENGRID_COLUMNS];
    int64_t time_ms_show;
    int64_t time_ms_hide;
    unsigned int cc_count;
    int service_number;
} tv_screen;
*/

typedef struct  cc_708_service_datapoints {

} cc_708_service_datapoints;

typedef struct cc_708_service_decoder {
    cc_708_window windows[CC_708_MAX_WINDOWS];
    int current_window;
    //tv_screen *tv;
    int cc_count;
} cc_708_service_decoder;


typedef struct cc_708_ctx {
    int is_active;
    int active_services_count;
    int services_active[CC_708_MAX_SERVICES]; //0 - inactive, 1 - active
    int report_enabled;

    //ccx_decoder_dtvcc_report *report;

    cc_708_service_decoder *decoders[CC_708_MAX_SERVICES];

    unsigned char current_packet[CCX_708_MAX_PACKET_LENGTH];
    int current_packet_length;

    //int last_sequence;

    void *encoder; //we can't include header, so keeping it this way
    int no_rollup;

    struct cc_common_timing_ctx *timing;

    AVFrameSideData *fsd;
    int prev_seq;
    int cur_service_number;
    
        
    unsigned char seq_mask;
    short seqcnt;

} cc_708_ctx;


typedef struct CCaption708SubContext {
    AVClass *class;

    int expected_cc_count;
    struct cc_decode *cc_decode;
    cc_708_ctx *cc708ctx;
    
    int start_of_channel_pkt;
    int end_of_channel_pkt;

} CCaption708SubContext;











#endif /* AVCODEC_CCAPTION708_DEC_H */
