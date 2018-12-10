#include "libavutil/opt.h"

#include "ccaption708_dec.h"
#include "cc_decoder_common.h"
#include "avcodec.h"
#include "libavutil/frame.h"

#define _CC_DEBUG

#ifdef _CC_DEBUGtiming
#define CC_DEBUG(s, v) av_log_set_level(AV_LOG_DEBUG); av_log(NULL, AV_LOG_DEBUG, s, v);
#else
#define CC_DEBUG(s)
#endif



const char *C0[32] = {
	"NUL", // 0 = NUL
	NULL,  // 1 = Reserved
	NULL,  // 2 = Reservedtiming
	"ETX", // 3 = ETXtiming
	NULL,  // 4 = Reserved
	NULL,  // 5 = Reserved
	NULL,  // 6 = Reserved
	NULL,  // 7 = Reserved
	"BS",  // 8 = Backspace
	NULL,  // 9 = Reserved
	NULL,  // A = Reserved
	NULL,  // B = Reserved
	"FF",  // C = FF
	"CR",  // D = CR
	"HCR", // E = HCR
	NULL,  // F = Reserved
	"EXT1",// 0x10 = EXT1,
	NULL,  // 0x11 = Reserved
	NULL,  // 0x12 = Reserved
	NULL,  // 0x13 = Reserved
	NULL,  // 0x14 = Reserved
	NULL,  // 0x15 = Reserved
	NULL,  // 0x16 = Reserved
	NULL,  // 0x17 = Reserved
	"P16", // 0x18 = P16
	NULL,  // 0x19 = Reserved
	NULL,  // 0x1A = Reserved
	NULL,  // 0x1B = Reserved
	NULL,  // 0x1C = Reserved
	NULL,  // 0x1D = Reserved
	NULL,  // 0x1E = Reserved
	NULL,  // 0x1F = Reserved
};

struct S_COMMANDS_C1 C1[32] = {
	{C1_CW0, "CW0", "SetCurrentWindow0",     1},
	{C1_CW1, "CW1", "SetCurrentWindow1",     1},
	{C1_CW2, "CW2", "SetCurrentWindow2",     1},
	{C1_CW3, "CW3", "SetCurrentWindow3",     1},
	{C1_CW4, "CW4", "SetCurrentWindow4",     1},
	{C1_CW5, "CW5", "SetCurrentWindow5",     1},
	{C1_CW6, "CW6", "SetCurrentWindow6",     1},
	{C1_CW7, "CW7", "SetCurrentWindow7",     1},
	{C1_CLW, "CLW", "ClearWindows",          2},
	{C1_DSW, "DSW", "DisplayWindows",        2},
	{C1_HDW, "HDW", "HideWindows",           2},
	{C1_TGW, "TGW", "ToggleWindows",         2},
	{C1_DLW, "DLW", "DeleteWindows",         2},
	{C1_DLY, "DLY", "Delay",                 2},
	{C1_DLC, "DLC", "DelayCancel",           1},
	{C1_RST, "RST", "Reset",                 1},
	{C1_SPA, "SPA", "SetPenAttributes",      3},
	{C1_SPC, "SPC", "SetPenColor",           4},
	{C1_SPL, "SPL", "SetPenLocation",        3},
	{C1_RSV93, "RSV93", "Reserved",          1},
	{C1_RSV94, "RSV94", "Reserved",          1},
	{C1_RSV95, "RSV95", "Reserved",          1},
	{C1_RSV96, "RSV96", "Reserved",          1},
	{C1_SWA, "SWA", "SetWindowAttributes",   5},
	{C1_DF0, "DF0", "DefineWindow0",         7},
	{C1_DF1, "DF1", "DefineWindow1",         7},
	{C1_DF2, "DF2", "DefineWindow2",         7},
	{C1_DF3, "DF3", "DefineWindow3",         7},
	{C1_DF4, "DF4", "DefineWindow4",         7},
	{C1_DF5, "DF5", "DefineWindow5",         7},
	{C1_DF6, "DF6", "DefineWindow6",         7},
	{C1_DF7, "DF7", "DefineWindow7",         7}
};

//------------------------- DEFAULT AND PREDEFINED -----------------------------

cc_708_pen_color cc_708_default_pen_color = {
	0x3f,
	0,
	0,
	0,
	0
};

cc_708_pen_attribs cc_708_default_pen_attribs =
{
	PEN_SIZE_STANDART,
	0,
	PEN_TEXT_TAG_UNDEFINED_12,
	0,
	PEN_EDGE_NONE,
	0,
	0
};

cc_708_window_attribs cc_708_predefined_window_styles[] =
{
	{0,0,0,0,0,0,0,0,0,0, 0}, // Dummy, unused (position 0 doesn't use the table)
	{//1 - NTSC Style PopUp Captions
		WINDOW_JUSTIFY_LEFT,
		WINDOW_PD_LEFT_RIGHT,
		WINDOW_SD_BOTTOM_TOP,
		0,
		WINDOW_SDE_SNAP,
		0,
		0,
		0,
		WINDOW_FO_SOLID,
		WINDOW_BORDER_NONE,
		0
	},
	{//2 - PopUp Captions w/o Black Background
		WINDOW_JUSTIFY_LEFT,
		WINDOW_PD_LEFT_RIGHT,
		WINDOW_SD_BOTTOM_TOP,
		0,
		WINDOW_SDE_SNAP,
		0,
		0,
		0,
		WINDOW_FO_TRANSPARENT,
		WINDOW_BORDER_NONE,
		0
	},
	{//3 - NTSC Style Centered PopUp Captions
		WINDOW_JUSTIFY_CENTER,
		WINDOW_PD_LEFT_RIGHT,
		WINDOW_SD_BOTTOM_TOP,
		0,
		WINDOW_SDE_SNAP,
		0,
		0,
		0,
		WINDOW_FO_SOLID,
		WINDOW_BORDER_NONE,
		0
	},
	{//4 - NTSC Style RollUp Captions
		WINDOW_JUSTIFY_LEFT,
		WINDOW_PD_LEFT_RIGHT,
		WINDOW_SD_BOTTOM_TOP,
		1,
		WINDOW_SDE_SNAP,
		0,
		0,
		0,
		WINDOW_FO_SOLID,
		WINDOW_BORDER_NONE,
		0
	},
	{//5 - RollUp Captions w/o Black Background
		WINDOW_JUSTIFY_LEFT,
		WINDOW_PD_LEFT_RIGHT,
		WINDOW_SD_BOTTOM_TOP,
		1,
		WINDOW_SDE_SNAP,
		0,
		0,
		0,
		WINDOW_FO_TRANSPARENT,
		WINDOW_BORDER_NONE,
		0
	},
	{//6 - NTSC Style Centered RollUp Captions
		WINDOW_JUSTIFY_CENTER,
		WINDOW_PD_LEFT_RIGHT,
		WINDOW_SD_BOTTOM_TOP,
		1,
		WINDOW_SDE_SNAP,
		0,
		0,
		0,
		WINDOW_FO_SOLID,
		WINDOW_BORDER_NONE,
		0
	},
	{//7 - Ticker tape
		WINDOW_JUSTIFY_LEFT,
		WINDOW_PD_TOP_BOTTOM,
		WINDOW_SD_RIGHT_LEFT,
		0,
		WINDOW_SDE_SNAP,
		0,
		0,
		0,
		WINDOW_FO_SOLID,
		WINDOW_BORDER_NONE,
		0
	}
};



//---------------------------------- COMMANDS ------------------------------------

static int _708_decoder_has_visible_windows(cc_708_service_decoder *decoder) {
	for (int i = 0; i < CC_708_MAX_WINDOWS; i++) {
		if (decoder->windows[i].visible)
			return 1;
	}
	return 0;
}

static void _708_handle_CWx_SetCurrentWindow(cc_708_service_decoder *decoder, int window_id)
{
	//ccx_common_logging.debug_ftn(
	//		CCX_DMT_708, "[CEA-708] dtvcc_handle_CWx_SetCurrentWindow: [%d]\n", window_id);
	if (decoder->windows[window_id].is_defined)
		decoder->current_window = window_id;
	//else
	//	ccx_common_logging.log_ftn("[CEA-708] dtvcc_handle_CWx_SetCurrentWindow: "
	//									   "window [%d] is not defined\n", window_id);
}

static void _708_window_clear_row(cc_708_window *window, int row_index)
{
	if (window->memory_reserved)
	{
		memset(window->rows[row_index], 0, CCX_708_MAX_COLUMNS * sizeof(cc_708_symbol));
		for (int column_index = 0; column_index < CCX_708_MAX_COLUMNS; column_index++)
		{
			window->pen_attribs[row_index][column_index] = cc_708_default_pen_attribs;
			window->pen_colors[row_index][column_index] = cc_708_default_pen_color;
		}
	}
}

static void _708_window_clear_text(cc_708_window *window)
{
	window->pen_color_pattern = cc_708_default_pen_color;
	window->pen_attribs_pattern = cc_708_default_pen_attribs;
	for (int i = 0; i < CCX_708_MAX_ROWS; i++)
		_708_window_clear_row(window, i);
	window->is_empty = 1;
}

static void _708_window_clear(cc_708_service_decoder *decoder, int window_id)
{
	_708_window_clear_text(&decoder->windows[window_id]);
	//OPT fill window with a window fill color
}


static void _708_window_apply_style(cc_708_window *window,
    cc_708_window_attribs *style)
{
	window->attribs.border_color = style->border_color;
	window->attribs.border_type = style->border_type;
	window->attribs.display_effect = style->display_effect;
	window->attribs.effect_direction = style->effect_direction;
	window->attribs.effect_speed = style->effect_speed;
	window->attribs.fill_color = style->fill_color;
	window->attribs.fill_opacity = style->fill_opacity;
	window->attribs.justify = style->justify;
	window->attribs.print_direction = style->print_direction;
	window->attribs.scroll_direction = style->scroll_direction;
	window->attribs.word_wrap = style->word_wrap;
}

static void _708_handle_CLW_ClearWindows(cc_708_service_decoder *decoder, int windows_bitmap)
{
    if (windows_bitmap != 0) {
		for (int i = 0; i < CC_708_MAX_WINDOWS; i++) {
			if (windows_bitmap & 1) {
				//ccx_common_logging.debug_ftn(CCX_DMT_708, "[W%d] ", i);
				_708_window_clear(decoder, i);
			}
			windows_bitmap >>= 1;
		}
	}
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "\n");
}

static size_t print_mstime_buff(int64_t mstime, char* fmt, char* buf) {
	unsigned hh, mm, ss, ms;
	int signoffset = (mstime < 0 ? 1 : 0);

	if (mstime < 0) // Avoid loss of data warning with abs()
		mstime = -mstime;

	hh = (unsigned) (mstime / 1000 / 60 / 60);
	mm = (unsigned) (mstime / 1000 / 60 - 60 * hh);
	ss = (unsigned) (mstime / 1000 - 60 * (mm + 60 * hh));
	ms = (unsigned) (mstime - 1000 * (ss + 60 * (mm + 60 * hh)));

	buf[0] = '-';

	return (size_t) sprintf(buf + signoffset, fmt, hh, mm, ss, ms);
}



/* This function returns a FTS that is guaranteed to be at least 1 ms later than the end of the previous screen. It shouldn't be needed
   obviously but it guarantees there's no timing overlap */
static int64_t get_visible_start (struct cc_common_timing_ctx *ctx,
    int current_field) {
	int64_t fts = get_fts(ctx, current_field);
	if (fts <= ctx->minimum_fts)
		fts = ctx->minimum_fts + 1;
	//ccx_common_logging.debug_ftn(CCX_DMT_DECODER_608, "Visible Start time=%s\n", print_mstime_static(fts));
	return fts;
}

/* This function returns the current FTS and saves it so it can be used by ctxget_visible_start */
static int64_t get_visible_end (struct cc_common_timing_ctx *ctx,
    int current_field) {
	int64_t fts = get_fts(ctx, current_field);
	if (fts > ctx->minimum_fts)
		ctx->minimum_fts = fts;
	//ccx_common_logging.debug_ftn(CCX_DMT_DECODER_608, "Visible End time=%s\n", print_mstime_static(fts));
	return fts;
}

static void _708_window_update_time_show(cc_708_window *window,
    struct cc_common_timing_ctx *timing) {
	char buf[128];
	window->time_ms_show = get_visible_start(timing, 3);
	//print_mstime_buff(window->time_ms_show, "%02u:%02u:%02u:%03u", buf);
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] "
	//		"[W-%d] show time updated to %s\n", window->number, buf);
}

static void _708_window_update_time_hide(cc_708_window *window,
    struct cc_common_timing_ctx *timing) {
	char buf[128];
	window->time_ms_hide = get_visible_end(timing, 3);
	//print_mstime_buff(window->time_ms_hide, "%02u:%02u:%02u:%03u", buf);
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] "
	//		"[W-%d] hide time updated to %s\n", window->number, buf);
}

static void _708_handle_DSW_DisplayWindows(cc_708_service_decoder *decoder,
    int windows_bitmap, struct cc_common_timing_ctx *timing) {
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] dtvcc_handle_DSW_DisplayWindows: windows: ");
	if (windows_bitmap == 0)
        ;
		//ccx_common_logging.debug_ftn(CCX_DMT_708, "none\n");
	else {
		for (int i = 0; i < CC_708_MAX_WINDOWS; i++) {
			if (windows_bitmap & 1) {
				//ccx_common_logging.debug_ftn(CCX_DMT_708, "[Window %d] ", i);
				if (!decoder->windows[i].is_defined) {
					//ccx_common_logging.log_ftn("[CEA-708] Error: window %d was not defined", i);
					continue;
				}
				if (!decoder->windows[i].visible) {
					decoder->windows[i].visible = 1;
					_708_window_update_time_show(&decoder->windows[i], timing);
				}
			}
			windows_bitmap >>= 1;
		}
		//ccx_common_logging.debug_ftn(CCX_DMT_708, "\n");
	}
}

static void _708_tv_cleartv(cc_708_service_decoder *decoder) {

};

static void _708_screen_print(cc_708_ctx *ctx,
    cc_708_service_decoder *decoder) {

	decoder->cc_count++;
	//_708_tv_clear(decoder);
}

static void _708_handle_HDW_HideWindows(cc_708_ctx *dtvcc,
								  cc_708_service_decoder *decoder,
								  int windows_bitmap)
{
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] dtvcc_handle_HDW_HideWindows: windows: ");
	if (windows_bitmap == 0)
        ;
	//	ccx_common_logging.debug_ftn(CCX_DMT_708, "none\n");
	else {
		int screen_content_changed = 0;
		for (int i = 0; i < CC_708_MAX_WINDOWS; i++) {
			if (windows_bitmap & 1) {
				//ccx_common_logging.debug_ftn(CCX_DMT_708, "[Window %d] ", i);
				if (decoder->windows[i].visible) {
					screen_content_changed = 1;
					decoder->windows[i].visible = 0;
					_708_window_update_time_hide(&decoder->windows[i], dtvcc->timing);
					//if (!decoder->windows[i].is_empty)
					//	_708_window_copy_to_screen(decoder, &decoder->windows[i]);
				}
			}
			windows_bitmap >>= 1;
		}
		//ccx_common_logging.debug_ftn(CCX_DMT_708, "\n");
		if (screen_content_changed && ! _708_decoder_has_visible_windows(decoder)){
                    _708_screen_print(dtvcc, decoder);
                }

	}
}

static void _708_handle_TGW_ToggleWindows(cc_708_ctx *dtvcc,
									cc_708_service_decoder *decoder,
									int windows_bitmap) {
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] dtvcc_handle_TGW_ToggleWindows: windows: ");
	if (windows_bitmap == 0)
        ;
        //	ccx_common_logging.debug_ftn(CCX_DMT_708, "none\n");
	else {
		int screen_content_changed = 0;
		for (int i = 0; i < CC_708_MAX_WINDOWS; i++) {
			cc_708_window *window = &decoder->windows[i];
			if ((windows_bitmap & 1) && window->is_defined) {
				//ccx_common_logging.debug_ftn(CCX_DMT_708, "[W-%d: %d->%d]", i, window->visible, !window->visible);
				window->visible = !window->visible;
				if (window->visible)
					_708_window_update_time_show(window, dtvcc->timing);
				else {
					_708_window_update_time_hide(window, dtvcc->timing);
					if (!window->is_empty) {
						screen_content_changed = 1;
						//_708_window_copy_to_screen(decoder, window);
					}
				}
			}
			windows_bitmap >>= 1;
		}
		//ccx_common_logging.debug_ftn(CCX_DMT_708, "\n");
		if (screen_content_changed && !_708_decoder_has_visible_windows(decoder))
			_708_screen_print(dtvcc, decoder);
	}
}

static void _708_check_window_position(cc_708_ctx *dtvcc, cc_708_window *window) {
	//"[CEA-708] _dtvcc_window_copy_to_screen: W-%d\n", window->number);

	// For each window we calculate the top, left position depending on the
	// anchor
        service_data_points *dp = &dtvcc->fsd->svcs_dp_708.svc_dps[dtvcc->cur_service_number];

	switch (window->anchor_point) {
		case ANCHOR_POINT_TOP_LEFT:

			break;
		case ANCHOR_POINT_TOP_CENTER:

			break;
		case ANCHOR_POINT_TOP_RIGHT:

			break;
		case ANCHOR_POINT_MIDDLE_LEFT:

			break;
		case ANCHOR_POINT_MIDDLE_CENTER:

			break;
		case ANCHOR_POINT_MIDDLE_RIGHT:

			break;
		case ANCHOR_POINT_BOTTOM_LEFT:

			break;
		case ANCHOR_POINT_BOTTOM_CENTER:

			break;
		case ANCHOR_POINT_BOTTOM_RIGHT:

			break;
		default: // Shouldn't happen, but skip the window just in case
                    dp->abnormal_window_position = 1;
			return;
			break;
        }
}


static void _708_handle_DFx_DefineWindow(cc_708_ctx *dtvcc, cc_708_service_decoder *decoder,
    int window_id, unsigned char *data, struct cc_common_timing_ctx *timing) {
	
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] dtvcc_handle_DFx_DefineWindow: "
	//		"W[%d], attributes: \n", window_id);
        service_data_points *dp = &dtvcc->fsd->svcs_dp_708.svc_dps[dtvcc->cur_service_number];
	cc_708_window *window = &decoder->windows[window_id];

	if (window->is_defined && !memcmp(window->commands, data + 1, 6)) {
		// When a decoder receives a DefineWindow command for an existing window, the
		// command is to be ignored if the command parameters are unchanged from the
		// previous window definition.
		//ccx_common_logging.debug_ftn(libavcodec/ccaption708_dec.c:515:23: error: ‘CCX_708_MAX_WINDOWS’ undeclared (first use in this function)
		//		CCX_DMT_708, "[CEA-708] dtvcc_handle_DFx_DefineWindow: Repeated window definition, ignored\n");
		return;
	}

	window->number = window_id;

	int priority = (data[1]) & 0x7;
	int col_lock = (data[1] >> 3) & 0x1;
	int row_lock = (data[1] >> 4) & 0x1;
	int visible  = (data[1] >> 5) & 0x1;
	int anchor_vertical = data[2] & 0x7f;
	int relative_pos = data[2] >> 7;
	int anchor_horizontal = data[3];
	int row_count = (data[4] & 0xf) + 1; //according to CEA-708-D
	int anchor_point = data[4] >> 4;
	int col_count = (data[5] & 0x3f) + 1; //according to CEA-708-D
	int pen_style = data[6] & 0x7;
	int win_style = (data[6] >> 3) & 0x7;

        if (row_count > 15 || col_count*2 > 64)
            dp->abnormal_window_size = 1;
        
         if (row_count > 15 || col_count > 32)
            dp->boundary_violation = 1;

	int do_clear_window = 0;

	if (anchor_vertical > CCX_708_SCREENGRID_ROWS - row_count)
		anchor_vertical = CCX_708_SCREENGRID_ROWS - row_count;
	if (anchor_horizontal > CCX_708_SCREENGRID_COLUMNS - col_count)
		anchor_horizontal = CCX_708_SCREENGRID_COLUMNS - col_count;

	window->priority = priority;
	window->col_lock = col_lock;
	window->row_lock = row_lock;
	window->visible = visible;
	window->anchor_vertical = anchor_vertical;
	window->relative_pos = relative_pos;
	window->anchor_horizontal = anchor_horizontal;
	window->row_count = row_count;
	window->anchor_point = anchor_point;
	window->col_count = col_count;


        _708_check_window_position(dtvcc, window);

	// If changing the style of an existing window delete contents
	if (win_style > 0 && window->is_defined && window->win_style != win_style)
		do_clear_window = 1;


	if (win_style == 0 && !window->is_defined) {
		win_style = 1;
	}

	if (pen_style == 0 && !window->is_defined) {
		pen_style = 1;
	}

	//Apply windows attribute presets
	if (win_style > 0 && win_style < 8)
        {
		window->win_style = win_style;
		window->attribs.border_color = cc_708_predefined_window_styles[win_style].border_color;
		window->attribs.border_type = cc_708_predefined_window_styles[win_style].border_type;
		window->attribs.display_effect = cc_708_predefined_window_styles[win_style].display_effect;
		window->attribs.effect_direction = cc_708_predefined_window_styles[win_style].effect_direction;
		window->attribs.effect_speed = cc_708_predefined_window_styles[win_style].effect_speed;
		window->attribs.fill_color = cc_708_predefined_window_styles[win_style].fill_color;
		window->attribs.fill_opacity = cc_708_predefined_window_styles[win_style].fill_opacity;
		window->attribs.justify = cc_708_predefined_window_styles[win_style].justify;
		window->attribs.print_direction = cc_708_predefined_window_styles[win_style].print_direction;
		window->attribs.scroll_direction = cc_708_predefined_window_styles[win_style].scroll_direction;
		window->attribs.word_wrap = cc_708_predefined_window_styles[win_style].word_wrap;
	}

	if (pen_style > 0) {
		//TODO apply static pen_style preset
		window->pen_style = pen_style;
	}

	if (!window->is_defined) {
		// If the window is being created, all character positions in the window
		// are set to the fill color and the pen location is set to (0,0)
		window->pen_column = 0;
		window->pen_row = 0;
		if (!window->memory_reserved) {
			for (int i = 0; i < CCX_708_MAX_ROWS; i++) {
				window->rows[i] = (cc_708_symbol *) malloc(CCX_708_MAX_COLUMNS * sizeof(cc_708_symbol));
                if (!window->rows[i]) {
                    ; //ccx_common_logging.fatal_ftn(EXIT_NOT_ENOUGH_MEMORY, "[CEA-708] dtvcc_handle_DFx_DefineWindow");
                    printf("ccaption708_dec.c::_708_handle_DFx_DefineWindow() [CEA-708] -> EXIT_NOT_ENOUGH_MEMORY\n");
                    abort();
                }
            }
			window->memory_reserved = 1;
		}
		window->is_defined = 1;
		_708_window_clear_text(window);

		//Accorgind to CEA-708-D if window_style is 0 for newly created window , we have to apply predefined style #1
		if (window->win_style == 0) {
			_708_window_apply_style(window, &cc_708_predefined_window_styles[0]);
		} else if (window->win_style <= 7) {
			_708_window_apply_style(window, &cc_708_predefined_window_styles[window->win_style - 1]);
		}
		else
		{
			//ccx_common_logging.log_ftn("[CEA-708] dtvcc_handle_DFx_DefineWindow: "
			//								   "invalid win_style num %d\n", window->win_style);
			_708_window_apply_style(window, &cc_708_predefined_window_styles[0]);
		}
	}
	else
	{
		if (do_clear_window)
			_708_window_clear_text(window);
	}
	// ...also makes the defined windows the current window
	_708_handle_CWx_SetCurrentWindow(decoder, window_id);
	memcpy(window->commands, data + 1, 6);

	if (window->visible)
		_708_window_update_time_show(window, dtvcc->timing);
	if (!window->memory_reserved) {
		for (int i = 0; i < CCX_708_MAX_ROWS; i++) {
			free(window->rows[i]);
		}
	}
	
}

static void _708_handle_SWA_SetWindowAttributes(cc_708_service_decoder *decoder,
    unsigned char *data) {
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] dtvcc_handle_SWA_SetWindowAttributes: attributes: \n");

	int fill_color    = (data[1]     ) & 0x3f;
	int fill_opacity  = (data[1] >> 6) & 0x03;
	int border_color  = (data[2]     ) & 0x3f;
	int border_type01 = (data[2] >> 6) & 0x03;
	int justify       = (data[3]     ) & 0x03;
	int scroll_dir    = (data[3] >> 2) & 0x03;
	int print_dir     = (data[3] >> 4) & 0x03;
	int word_wrap     = (data[3] >> 6) & 0x01;
	int border_type   = ((data[3] >> 5) & 0x04)| border_type01;
	int display_eff   = (data[4]     ) & 0x03;
	int effect_dir    = (data[4] >> 2) & 0x03;
	int effect_speed  = (data[4] >> 4) & 0x0f;

#if 0
	ccx_common_logging.debug_ftn(CCX_DMT_708, "       Fill color: [%d]     Fill opacity: [%d]    Border color: [%d]  Border type: [%d]\n",
			fill_color, fill_opacity, border_color, border_type01);
	ccx_common_logging.debug_ftn(CCX_DMT_708, "          Justify: [%d]       Scroll dir: [%d]       Print dir: [%d]    Word wrap: [%d]\n",
			justify, scroll_dir, print_dir, word_wrap);
	ccx_common_logging.debug_ftn(CCX_DMT_708, "      Border type: [%d]      Display eff: [%d]      Effect dir: [%d] Effect speed: [%d]\n",
			border_type, display_eff, effect_dir, effect_speed);
#endif

	if (decoder->current_window == -1)
	{
	//	ccx_common_logging.log_ftn("[CEA-708] dtvcc_handle_SWA_SetWindowAttributes: "
	//									   "Window has to be defined first\n");
		return;
	}

	cc_708_window *window = &decoder->windows[decoder->current_window];

	window->attribs.fill_color = fill_color;
	window->attribs.fill_opacity = fill_opacity;
	window->attribs.border_color = border_color;
	window->attribs.justify = justify;
	window->attribs.scroll_direction = scroll_dir;
	window->attribs.print_direction = print_dir;
	window->attribs.word_wrap = word_wrap;
	window->attribs.border_type = border_type;
	window->attribs.display_effect = display_eff;
	window->attribs.effect_direction = effect_dir;
	window->attribs.effect_speed = effect_speed;
}

static void _708_handle_DLW_DeleteWindows(cc_708_ctx *dtvcc,
	cc_708_service_decoder *decoder, int windows_bitmap) {
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] dtvcc_handle_DLW_DeleteWindows: windows: ");

	int screen_content_changed = 0,
		window_had_content;
	if (windows_bitmap == 0)
		;//ccx_common_logging.debug_ftn(CCX_DMT_708, "none\n");
	else {
		for (int i = 0; i < CC_708_MAX_WINDOWS; i++) {
			if (windows_bitmap & 1) {
				cc_708_window *window = &decoder->windows[i];
				//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] Deleting [W-%d]\n", i);
				window_had_content = window->is_defined && window->visible && !window->is_empty;
				if (window_had_content) {
					screen_content_changed = 1;
					_708_window_update_time_hide(window, dtvcc->timing);
					//_708_window_copy_to_screen(decoder, &decoder->windows[i]);
				}
				decoder->windows[i].is_defined = 0;
				decoder->windows[i].visible = 0;
				decoder->windows[i].time_ms_hide = -1;
				decoder->windows[i].time_ms_show = -1;
				if (i == decoder->current_window)
				{
					// If the current window is deleted, then the decoder's current window ID
					// is unknown and must be reinitialized with either the SetCurrentWindow
					// or DefineWindow command.
					decoder->current_window = -1;
				}
			}
			windows_bitmap >>= 1;
		}
	}
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "\n");
	if (screen_content_changed && !_708_decoder_has_visible_windows(decoder))
		_708_screen_print(dtvcc, decoder);
}

static void _708_handle_SPA_SetPenAttributes(cc_708_service_decoder *decoder,
    unsigned char *data) {
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] dtvcc_handle_SPA_SetPenAttributes: attributes: \n");

	int pen_size  = (data[1]     ) & 0x3;
	int offset    = (data[1] >> 2) & 0x3;
	int text_tag  = (data[1] >> 4) & 0xf;
	int font_tag  = (data[2]     ) & 0x7;
	int edge_type = (data[2] >> 3) & 0x7;
	int underline = (data[2] >> 6) & 0x1;
	int italic    = (data[2] >> 7) & 0x1;

#if 0
	ccx_common_logging.debug_ftn(CCX_DMT_708, "       Pen size: [%d]     Offset: [%d]  Text tag: [%d]   Font tag: [%d]\n",
			pen_size, offset, text_tag, font_tag);
	ccx_common_logging.debug_ftn(CCX_DMT_708, "      Edge type: [%d]  Underline: [%d]    Italic: [%d]\n",
			edge_type, underline, italic);
#endif

	if (decoder->current_window == -1)

	{
		//ccx_common_logging.log_ftn("[CEA-708] dtvcc_handle_SPA_SetPenAttributes: "
		//								   "Window has to be defined first\n");
		return;
	}

	cc_708_window *window = &decoder->windows[decoder->current_window];

	if (window->pen_row == -1)
	{
		//ccx_common_logging.log_ftn("[CEA-708] dtvcc_handle_SPA_SetPenAttributes: "
		//								   "can't set pen attribs for undefined row\n");
		return;
	}

	cc_708_pen_attribs *pen = &window->pen_attribs_pattern;

	pen->pen_size = pen_size;
	pen->offset = offset;
	pen->text_tag = text_tag;
	pen->font_tag = font_tag;
	pen->edge_type = edge_type;
	pen->underline = underline;
	pen->italic = italic;
}

static void _708_handle_SPC_SetPenColor(cc_708_service_decoder *decoder,
    unsigned char *data) {
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] dtvcc_handle_SPC_SetPenColor: attributes: \n");

	int fg_color   = (data[1]     ) & 0x3f;
	int fg_opacity = (data[1] >> 6) & 0x03;
	int bg_color   = (data[2]     ) & 0x3f;
	int bg_opacity = (data[2] >> 6) & 0x03;
	int edge_color = (data[3]     ) & 0x3f;

#if 0
	ccx_common_logging.debug_ftn(CCX_DMT_708, "      Foreground color: [%d]     Foreground opacity: [%d]\n",
			fg_color, fg_opacity);
	ccx_common_logging.debug_ftn(CCX_DMT_708, "      Background color: [%d]     Background opacity: [%d]\n",
			bg_color, bg_opacity);
	ccx_common_logging.debug_ftn(CCX_DMT_708, "            Edge color: [%d]\n",
			edge_color);
#endif

	if (decoder->current_window == -1) {
	//	ccx_common_logging.log_ftn("[CEA-708] dtvcc_handle_SPC_SetPenColor: "
	//									   "Window has to be defined first\n");
		return;
	}

	cc_708_window *window = &decoder->windows[decoder->current_window];

	if (window->pen_row == -1) {
		//ccx_common_logging.log_ftn("[CEA-708] dtvcc_handle_SPA_SetPenAttributes: "
		//								   "can't set pen color for undefined row\n");
		return;
	}

	cc_708_pen_color *color = &window->pen_color_pattern;

	color->fg_color = fg_color;
	color->fg_opacity = fg_opacity;
	color->bg_color = bg_color;
	color->bg_opacity = bg_opacity;
	color->edge_color = edge_color;
}

static void _708_handle_SPL_SetPenLocation(cc_708_service_decoder *decoder,
    unsigned char *data) {
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] dtvcc_handle_SPL_SetPenLocation: attributes: \n");

	int row = data[1] & 0x0f;
	int col = data[2] & 0x3f;

	//ccx_common_logging.debug_ftn(CCX_DMT_708, "      row: [%d]     Column: [%d]\n", row, col);

	if (decoder->current_window == -1) {
		//ccx_common_logging.log_ftn("[CEA-708] dtvcc_handle_SPL_SetPenLocation: "
		//								   "Window has to be defined first\n");
		return;
	}

	cc_708_window *window = &decoder->windows[decoder->current_window];
	window->pen_row = row;
	window->pen_column = col;
}

static void cc_708_windows_reset(cc_708_service_decoder *decoder) {
	for (int j = 0; j < CC_708_MAX_WINDOWS; j++) {
		_708_window_clear_text(&decoder->windows[j]);
		decoder->windows[j].is_defined = 0;
		decoder->windows[j].visible = 0;
		memset(decoder->windows[j].commands, 0, sizeof(decoder->windows[j].commands));
	}
	decoder->current_window = -1;
	//_708_tv_clear(decoder);
}

static void cc_708_clear_packet(cc_708_ctx *ctx) {
	ctx->current_packet_length = 0;
	memset(ctx->current_packet, 0, CCX_708_MAX_PACKET_LENGTH * sizeof(unsigned char));
}

static void _708_decoders_reset(cc_708_ctx *dtvcc) {

	for (int i = 0; i < CC_708_MAX_SERVICES; i++) {
		if (!dtvcc->services_active[i])
			continue;
		cc_708_windows_reset(dtvcc->decoders[i]);
	}

	cc_708_clear_packet(dtvcc);

//	dtvcc->last_sequence = CCX_708_NO_LAST_SEQUENCE;
	//dtvcc->report->reset_count++;
}

//static void _708_handle_RST_Reset(cc_708_service_decoder *decoder) {
//	cc_708_windows_reset(decoder);
//}

//END COMMANDS

////
static unsigned char get_internal_from_G0(unsigned char g0_char) {
	return g0_char;
}

static unsigned char get_internal_from_G1(unsigned char g1_char) {
	return g1_char;
}

// TODO: Probably not right
// G2: Extended Control Code Set 1
static unsigned char get_internal_from_G2(unsigned char g2_char){
	if (g2_char >= 0x20 && g2_char <= 0x3F)
		return g2_char - (unsigned char)0x20;
	if (g2_char >= 0x60 && g2_char <= 0x7F)
		return g2_char + (unsigned char)0x20;
	// Rest unmapped, so we return a blank space
	return 0x20;
}

// TODO: Probably not right
// G3: Future Characters and Icon Expansion
static unsigned char _708_get_internal_from_G3(unsigned char g3_char) {
	if (g3_char == 0xa0) // The "CC" (closed captions) sign
		return 0x06;
	// Rest unmapped, so we return a blank space
	return 0x20;
}


static void init_data_points(AVFrameSideData *fsd) {
    cc_708_channel_datapoints *chan_dp = &fsd->channel_dp_708;
    cc_708_services  *svc_dp = &fsd->svcs_dp_708;
    int k = 0;
    chan_dp->dtvcc_packing_matched = 1;
    chan_dp->sequence_continuity = 1;
    chan_dp->packet_errors = 0;
    chan_dp->packet_loss = 0;
    
    svc_dp->abnormal_service_block = 0;

    for(; k < CC_708_MAX_SERVICES; k++) {
        svc_dp->service_number[k] = 0;
        svc_dp->svc_dps[k].svc_type = None_SVC;
        svc_dp->svc_dps[k].abnormal_window_size = 0;
        svc_dp->svc_dps[k].abnormal_window_position = 0;
        svc_dp->svc_dps[k].abnormal_control_codes = 0;
        svc_dp->svc_dps[k].abnormal_characters = 0;
        svc_dp->svc_dps[k].boundary_violation = 0;
    }
}

////
static int cc_708_init(AVCodecContext *avctx) {
    CCaption708SubContext *ccsubctxt = (CCaption708SubContext*)avctx->priv_data;
    cc_708_service_decoder *decoder;
    cc_708_ctx *ctx = (cc_708_ctx *)malloc(sizeof(cc_708_ctx));
    
    if (!ctx)
        return -1;
    
    ccsubctxt->cc_decode = init_cc_decode();
    ctx->timing = ccsubctxt->cc_decode->timing;
    size_t k = 0;
    ccsubctxt->end_of_channel_pkt = 0;
    ctx->prev_seq = -1;

    
    ctx->seq_mask = 0;
    ctx->seqcnt = 0;
    ccsubctxt->cc708ctx = ctx;

    //ctx->report = opts->report;
	//ctx->report->reset_count = 0;

    ctx->is_active = 0;
	ctx->report_enabled = 0;
	ctx->no_rollup = 1;
	ctx->active_services_count = 2; //opts->active_services_count;

    //TODO:find a way to enable services
    //For now enable all services

    while (k < CC_708_MAX_SERVICES) {
		ctx->decoders[k] = NULL;
       ctx->services_active[k++] = 0;
    }

    cc_708_clear_packet(ctx);

//	ctx->last_sequence = CCX_708_NO_LAST_SEQUENCE;

	//ctx->report_enabled = opts->print_file_reports;
	//ctx->timing = opts->timing;


	for (int i = 0; i < CC_708_MAX_SERVICES; i++) {
		if (!ctx->services_active[i])
			continue;

		/* // the following part init cc_decoder, 
		   // move this part to where malloc decoder
		decoder = ctx->decoders[i];
		decoder->cc_count = 0;

		for (int j = 0; j < CC_708_MAX_WINDOWS; j++)
			decoder->windows[j].memory_reserved = 0;

		cc_708_windows_reset(decoder);
		*/
	}

    //pass option or something to enable it.
    ctx->is_active = 1;//setting->settings_dtvcc->enabled;
    return 0;
}

static av_cold int init_decoder(AVCodecContext *avctx) {
    if (cc_708_init(avctx))
        return -1;

    return 0;
}

static av_cold int close_decoder(AVCodecContext *avctx) {
	return 0;
}

static void flush_decoder(AVCodecContext *avctx) {

}

static int validate_cc_data_pair(unsigned char *cc_data_pair) {

    unsigned char cc_valid = (*cc_data_pair & 4) >>2;
    unsigned char cc_type = *cc_data_pair & 3;
    printf("cc_valid : %d  cctype : %d \n", cc_valid, cc_type);

   // if (!cc_valid)
   // 	return -1;

	return 0;
}

static void _708_process_character(cc_708_service_decoder *decoder, cc_708_symbol symbol)
{
	//"[CEA-708] %d\n", decoder->current_window);
	int cw = decoder->current_window;
	cc_708_window *window = &decoder->windows[cw];


    /**
    ccx_common_logging.debug_ftn(
			CCX_DMT_708, "[CEA-708] _dtvcc_process_character: "
					"%c [%02X]  - Window: %d %s, Pen: %d:%d\n",
			CCX_DTVCC_SYM(symbol), CCX_DTVCC_SYM(symbol),
			cw, window->is_defined ? "[OK]" : "[undefined]",
			cw != -1 ? window->pen_row : -1, cw != -1 ? window->pen_column : -1
	);
    */

	if (cw == -1 || !window->is_defined) // Writing to a non existing window, skipping
		return;

	window->is_empty = 0;
	window->rows[window->pen_row][window->pen_column] = symbol;
	window->pen_attribs[window->pen_row][window->pen_column] = window->pen_attribs_pattern;		// "Painting" char by pen - attribs
	window->pen_colors[window->pen_row][window->pen_column] = window->pen_color_pattern;		// "Painting" char by pen - colors
	switch (window->attribs.print_direction) {
		case WINDOW_PD_LEFT_RIGHT:
			if (window->pen_column + 1 < window->col_count)
				window->pen_column++;
			break;
		case WINDOW_PD_RIGHT_LEFT:
			if (decoder->windows->pen_column > 0)
				window->pen_column--;
			break;
		case WINDOW_PD_TOP_BOTTOM:
			if (window->pen_row + 1 < window->row_count)
				window->pen_row++;
			break;
		case WINDOW_PD_BOTTOM_TOP:
			if (window->pen_row > 0)
				window->pen_row--;
			break;
		default:
			//"[CEA-708] _dtvcc_process_character: unhandled branch (%02d)\n",
			//	window->attribs.print_direction);
			break;
	}
}

static unsigned char _708_get_internal_from_G1(unsigned char g1_char)
{
	return g1_char;
}

// TODO: Probably not right
// G2: Extended Control Code Set 1
static unsigned char _708_get_internal_from_G2(unsigned char g2_char) {
	if (g2_char >= 0x20 && g2_char <= 0x3F)
		return g2_char - (unsigned char)0x20;
	if (g2_char >= 0x60 && g2_char <= 0x7F)
		return g2_char + (unsigned char)0x20;
	// Rest unmapped, so we return a blank space
	return 0x20;
}


/* This function handles future codes. While by definition we can't do any work on them, we must return
   how many bytes would be consumed if these codes were supported, as defined in the specs.
Note: EXT1 not included */
// C2: Extended Miscellaneous Control Codes
// WARN: This code is completely untested due to lack of samples. Just following specs!
static int _708_handle_C2(cc_708_service_decoder *decoder, unsigned char *data, int data_length)
{
	if (data[0] <= 0x07) // 00-07...
		return 1; // ... Single-byte control bytes (0 additional bytes)
	else if (data[0] <= 0x0f) // 08-0F ...
		return 2; // ..two-byte control codes (1 additional byte)
	else if (data[0] <= 0x17)  // 10-17 ...
		return 3; // ..three-byte control codes (2 additional bytes)
	return 4; // 18-1F => four-byte control codes (3 additional bytes)
}

static int _708_handle_C3(cc_708_service_decoder *decoder, unsigned char *data, int data_length)
{
	if (data[0] < 0x80 || data[0] > 0x9F)
		//ccx_common_logging.fatal_ftn(
		;//		CCX_COMMON_EXIT_BUG_BUG, "[CEA-708] Entry in _dtvcc_handle_C3 with an out of range value.");
	if (data[0] <= 0x87) // 80-87...
		return 5; // ... Five-byte control bytes (4 additional bytes)
	else if (data[0] <= 0x8F) // 88-8F ...
		return 6; // ..Six-byte control codes (5 additional byte)
	// If here, then 90-9F ...

	// These are variable length commands, that can even span several segments
	// (they allow even downloading fonts or graphics).
	//ccx_common_logging.fatal_ftn(
	//		CCX_COMMON_EXIT_UNSUPPORTED, "[CEA-708] This sample contains unsupported 708 data. "
	//		"PLEASE help us improve CCExtractor by submitting it.\n");
	return 0; // Unreachable, but otherwise there's compilers warnings
}


// This function handles extended codes (EXT1 + code), from the extended sets
// G2 (20-7F) => Mostly unmapped, except for a few characters.
// G3 (A0-FF) => A0 is the CC symbol, everything else reserved for future expansion in EIA708-B
// C2 (00-1F) => Reserved for future extended misc. control and captions command codes
// WARN: This code is completely untested due to lack of samples. Just following specs!
// Returns number of used bytes, usually 1 (since EXT1 is not counted).
static int _708_handle_extended_char(cc_708_service_decoder *decoder,
    unsigned char *data, int data_length) {
	int used;
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] In _dtvcc_handle_extended_char, "
	//		"first data code: [%c], length: [%u]\n", data[0], data_length);
	unsigned char c = 0x20; // Default to space
	unsigned char code = data[0];
	if (/* data[i]>=0x00 && */ code <= 0x1F) {// Comment to silence warning
		used = _708_handle_C2(decoder, data, data_length);
	}
		// Group G2 - Extended Miscellaneous Characters
	else if (code >= 0x20 && code <= 0x7F)
	{
		c = _708_get_internal_from_G2(code);
		used = 1;
		cc_708_symbol sym;
		SYM_SET(sym, c);
		_708_process_character(decoder, sym);
	}
		// Group C3
	else if (code>= 0x80 && code <= 0x9F) {
		used = _708_handle_C3(decoder, data, data_length);
		// TODO: Something
	}else {// Group G3
	c = _708_get_internal_from_G3(code);
		used = 1;
		cc_708_symbol sym;
		SYM_SET(sym, c);
		_708_process_character(decoder, sym);
	}
	return used;
}

// G0 - Code Set - ASCII printable characters
static int _708_handle_G0(cc_708_service_decoder *decoder, unsigned char *data,
    int data_length) {

    unsigned char c;

    if (decoder->current_window == -1) {
		//"[CEA-708] _dtvcc_handle_G0: Window has to be defined first\n");
		return data_length;
	}

	c = data[0];
	//"[CEA-708] G0: [%02X]  (%c)\n", c, c);
	cc_708_symbol sym;
	if (c == 0x7F) {	// musical note replaces the Delete command code in ASCII
		SYM_SET(sym, CCX_708_MUSICAL_NOTE_CHAR);
	} else {
		unsigned char uc = get_internal_from_G0(c);
		SYM_SET(sym, uc);
	}
	_708_process_character(decoder, sym);
	return 1;
}

// G1 Code Set - ISO 8859-1 LATIN-1 Character Set
static int _708_handle_G1(cc_708_service_decoder *decoder, unsigned char *data,
    int data_length) {
	//"[CEA-708] G1: [%02X]  (%c)\n", data[0], data[0]);
	unsigned char c = get_internal_from_G1(data[0]);
	cc_708_symbol sym;
	SYM_SET(sym, c);
	_708_process_character(decoder, sym);
	return 1;
}

static void _708_process_hcr(cc_708_service_decoder *decoder) {
	if (decoder->current_window == -1) {
		//"[CEA-708] _dtvcc_process_hcr: Window has to be defined first\n");
		return;
	}

	cc_708_window *window = &decoder->windows[decoder->current_window];
	window->pen_column = 0;
	_708_window_clear_row(window, window->pen_row);
}

static void _708_process_ff(cc_708_service_decoder *decoder) {
	if (decoder->current_window == -1) {
		//"[CEA-708] _dtvcc_process_ff: Window has to be defined first\n");
		return;
	}
	cc_708_window *window = &decoder->windows[decoder->current_window];
	window->pen_column = 0;
	window->pen_row = 0;
	//CEA-708-D doesn't say we have to clear neither window text nor text line,
	//but it seems we have to clean the line
	//_dtvcc_window_clear_text(window);
}

static void _708_process_etx(cc_708_service_decoder *decoder) {
	
}

static void _708_window_rollup(cc_708_service_decoder *decoder,
    cc_708_window *window) {
	for (int i = 0; i < window->row_count - 1; i++)	{
		memcpy(window->rows[i], window->rows[i + 1],
            CCX_708_MAX_COLUMNS * sizeof(cc_708_symbol));

        for (int z = 0; z < CCX_708_MAX_COLUMNS; z++) {
			window->pen_colors[i][z] = window->pen_colors[i + 1][z];
			window->pen_attribs[i][z] = window->pen_attribs[i + 1][z];
		}
	}

	_708_window_clear_row(window, window->row_count - 1);
}


static void _708_process_cr(cc_708_ctx *ctx, cc_708_service_decoder *decoder) {
	if (decoder->current_window == -1) {
		//"[CEA-708] _dtvcc_process_cr: Window has to be defined first\n");
		return;
	}

	cc_708_window *window = &decoder->windows[decoder->current_window];

	int rollup_required = 0;
	switch (window->attribs.print_direction) {
		case WINDOW_PD_LEFT_RIGHT:
			window->pen_column = 0;
			if (window->pen_row + 1 < window->row_count)
				window->pen_row++;
			else rollup_required = 1;
			break;
		case WINDOW_PD_RIGHT_LEFT:
			window->pen_column = window->col_count;
			if (window->pen_row + 1 < window->row_count)
				window->pen_row++;
			else rollup_required = 1;
			break;
		case WINDOW_PD_TOP_BOTTOM:
			window->pen_row = 0;
			if (window->pen_column + 1 < window->col_count)
				window->pen_column++;
			else rollup_required = 1;
			break;
		case WINDOW_PD_BOTTOM_TOP:
			window->pen_row = window->row_count;
			if (window->pen_column + 1 < window->col_count)
				window->pen_column++;
			else rollup_required = 1;
			break;
		default:
			//"[CEA-708] _dtvcc_process_cr: unhandled branch\n");
			break;
	}

	if (window->is_defined)	{
		//"[CEA-708] _dtvcc_process_cr: rolling up\n");

		//_708_window_update_time_hide(window, dtvcc->timing);
		//_708_window_copy_to_screen(decoder, window);
		_708_screen_print(ctx, decoder);

		if (rollup_required) {
			if (ctx->no_rollup)
				_708_window_clear_row(window, window->pen_row);
			else
				_708_window_rollup(decoder, window);
		}
		//_dtvcc_window_update_time_show(window, dtvcc->timing);
	}
}

static void _708_process_bs(cc_708_service_decoder *decoder) {
	if (decoder->current_window == -1) {
		//ccx_common_logging.log_ftn("[CEA-708] _dtvcc_process_bs: Window has to be defined first\n");
		return;
	}

	//it looks strange, but in some videos (rarely) we have a backspace command
	//we just print one character over another
	int cw = decoder->current_window;
	cc_708_window *window = &decoder->windows[cw];

	switch (window->attribs.print_direction)
	{
	case WINDOW_PD_RIGHT_LEFT:
		if (window->pen_column + 1 < window->col_count)
			window->pen_column++;
		break;
	case WINDOW_PD_LEFT_RIGHT:
		if (decoder->windows->pen_column > 0)
			window->pen_column--;
		break;
	case WINDOW_PD_BOTTOM_TOP:
		if (window->pen_row + 1 < window->row_count)
			window->pen_row++;
		break;
	case WINDOW_PD_TOP_BOTTOM:
		if (window->pen_row > 0)
			window->pen_row--;
		break;
	default:
		//ccx_common_logging.log_ftn("[CEA-708] _dtvcc_process_character: unhandled branch (%02d)\n",
		//	window->attribs.print_direction);
		break;
	}
}

static int _708_handle_C0_P16(cc_708_service_decoder *decoder,
    unsigned char *data) {//16-byte chars always have 2 bytes

	if (decoder->current_window == -1) {
		//ccx_common_logging.log_ftn("[CEA-708] _dtvcc_handle_C0_P16: Window has to be defined first\n");
		return 3;
	}

	cc_708_symbol sym;

	if (data[0]) {
		SYM_SET_16(sym, data[0], data[1]);
	} else {
		SYM_SET(sym, data[1]);
	}

	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] _dtvcc_handle_C0_P16: [%04X]\n", sym.sym);
	_708_process_character(decoder, sym);

	return 3;
}

static int _708_handle_C0(cc_708_ctx *ctx, cc_708_service_decoder *decoder,
    unsigned char *data, int data_length) {

    unsigned char c0 = data[0];
	const char *name = C0[c0];
	if (name == NULL)
		name = "Reserved";

    service_data_points *dp = &ctx->fsd->svcs_dp_708.svc_dps[ctx->cur_service_number];
	//"[CEA-708] C0: [%02X]  (%d)   [%s]\n", c0, data_length, name);

	int len = -1;
	// These commands have a known length even if they are reserved.
	if (c0 <= 0xF) {
		switch (c0) {
			case C0_NUL:
				// No idea what they use NUL for, specs say they come from ASCII,
				// ASCII say it's "do nothing"
				break;
			case C0_CR:
				_708_process_cr(ctx, decoder);
				break;
			case C0_HCR:
				_708_process_hcr(decoder);
				break;
			case C0_FF:
				_708_process_ff(decoder);
				break;
			case C0_ETX:
				_708_process_etx(decoder);
				break;
			case C0_BS:
				_708_process_bs(decoder);
				break;
			default:
                            dp->abnormal_control_codes = 1;
				//"[CEA-708] _dtvcc_handle_C0: unhandled branch\n");
				break;
		}
		len = 1;
	}
	else if (c0 >= 0x10 && c0 <= 0x17) {
		// Note that 0x10 is actually EXT1 and is dealt with somewhere else. Rest is undefined as per
		// CEA-708-D
		len = 2;
	} else if (c0 >= 0x18 && c0 <= 0x1F) {
		if (c0 == C0_P16) // PE16
			_708_handle_C0_P16(decoder, data + 1);
		len = 3;
	}
	if (len == -1) {
                 dp->abnormal_control_codes = 1;
		//"[CEA-708] _dtvcc_handle_C0: impossible len == -1");
		return -1;
	}
	if (len > data_length) {
            dp->abnormal_control_codes = 1;
		//"[CEA-708] _dtvcc_handle_C0: "
		//"command is %d bytes long but we only have %d\n", len, data_length);
		return -1;
	}
	return len;
}

//------------------------- SYNCHRONIZATION COMMANDS -------------------------

static void _708_handle_DLY_Delay(cc_708_service_decoder *decoder,
    int tenths_of_sec) {
//	ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] dtvcc_handle_DLY_Delay: "
//			"delay for [%d] tenths of second", tenths_of_sec);
	// TODO: Probably ask for the current FTS and wait for this time before resuming - not sure it's worth it though
	// TODO: No, seems to me that idea above will not work
}

static void _708_handle_DLC_DelayCancel(cc_708_service_decoder *decoder) {
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] dtvcc_handle_DLC_DelayCancel");
	// TODO: See above
}

//-------------------------- CHARACTERS AND COMMANDS -------------------------


// C1 Code Set - Captioning Commands Control Codes
static int _708_handle_C1(cc_708_ctx *dtvcc,
					 cc_708_service_decoder *decoder,
					 unsigned char *data,
					 int data_length) {


    service_data_points *dp = &dtvcc->fsd->svcs_dp_708.svc_dps[dtvcc->cur_service_number];

    struct S_COMMANDS_C1 com = C1[data[0] - 0x80];
	//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] C1: %s | [%02X]  [%s] [%s] (%d)\n",
	//		print_mstime_static(get_fts(dtvcc->timing, 3)),
	//		data[0], com.name, com.description, com.length);

	if (com.length > data_length) {
		//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] C1: Warning: Not enough bytes for command.\n");
		return -1;
	}

	switch (com.code) {
		case C1_CW0: /* SetCurrentWindow */
		case C1_CW1:
		case C1_CW2:
		case C1_CW3:
		case C1_CW4:
		case C1_CW5:
		case C1_CW6:
		case C1_CW7:
			_708_handle_CWx_SetCurrentWindow(decoder, com.code - C1_CW0); /* Window 0 to 7 */
			break;
		case C1_CLW:
			_708_handle_CLW_ClearWindows(decoder, data[1]);
			break;
		case C1_DSW:
			_708_handle_DSW_DisplayWindows(decoder, data[1], dtvcc->timing);
			break;
		case C1_HDW:
			_708_handle_HDW_HideWindows(dtvcc, decoder, data[1]);
			break;
		case C1_TGW:
			_708_handle_TGW_ToggleWindows(dtvcc, decoder, data[1]);
			break;
		case C1_DLW:
			_708_handle_DLW_DeleteWindows(dtvcc, decoder, data[1]);
			break;
		case C1_DLY:
			_708_handle_DLY_Delay(decoder, data[1]);
			break;
		case C1_DLC:
			_708_handle_DLC_DelayCancel(decoder);
			break;
		case C1_RST:
			//_708_handle_RST_Reset(decoder);
			break;
		case C1_SPA:
			_708_handle_SPA_SetPenAttributes(decoder, data);
			break;
		case C1_SPC:
			_708_handle_SPC_SetPenColor(decoder, data);
			break;
		case C1_SPL:
			_708_handle_SPL_SetPenLocation(decoder, data);
			break;
		case C1_RSV93:
		case C1_RSV94:
		case C1_RSV95:
		case C1_RSV96:
			//ccx_common_logging.debug_ftn(CCX_DMT_708, "[CEA-708] Warning, found Reserved codes, ignored.\n");
			break;
		case C1_SWA:
			_708_handle_SWA_SetWindowAttributes(decoder, data);
			break;
		case C1_DF0:
		case C1_DF1:
		case C1_DF2:
		case C1_DF3:
		case C1_DF4:
		case C1_DF5:
		case C1_DF6:
		case C1_DF7:
			//_708_handle_DFx_DefineWindow(decoder, com.code - C1_DF0, data, dtvcc->timing); /* Window 0 to 7 */
			_708_handle_DFx_DefineWindow(dtvcc, decoder, com.code - C1_DF0, data, NULL);
            break;
		default:
                    dp->abnormal_control_codes = 1;
			//ccx_common_logging.log_ftn ("[CEA-708] BUG: Unhandled code in _dtvcc_handle_C1.\n");
			break;
	}

	return com.length;
}

static void cc_708_process_service_block(cc_708_ctx *cc708ctx,
    cc_708_service_decoder *decoder, unsigned char *data, int data_length) {

    int i = 0;

    //service block length check length <= 31 bytes
    cc_708_services *dp = &cc708ctx->fsd->svcs_dp_708;
    if (data_length > 31)
        dp->abnormal_service_block = 1;

	while (i < data_length)	{
		int used = -1;
		if (data[i] != C0_EXT1) {
			if (data[i] <= 0x1F)
				used = _708_handle_C0(cc708ctx, decoder, data + i, data_length - i);
			else if (data[i] >= 0x20 && data[i] <= 0x7F)
				used = _708_handle_G0(decoder, data + i, data_length - i);
			else if (data[i] >= 0x80 && data[i] <= 0x9F)
				used = _708_handle_C1(cc708ctx, decoder, data + i, data_length - i);
			else
				used = _708_handle_G1(decoder, data + i, data_length - i);

			if (used == -1) {
				//"[CEA-708] ccx_dtvcc_process_service_block: "
				//"There was a problem handling the data. Reseting service decoder\n");
				// TODO: Not sure if a local reset is going to be helpful here.
				//ccx_dtvcc_windows_reset(decoder);
				return;
			}
		} else { // Use extended set
			used = _708_handle_extended_char(decoder, data + i + 1, data_length - 1);
			used++; // Since we had CCX_DTVCC_C0_EXT1
		}
		i += used;
	}
}

static void cc_708_process_current_data(cc_708_ctx *cc708ctx) {
    unsigned char *pos;
    // Two most significants bits
    int seq = (cc708ctx->current_packet[0] & 0xC0) >> 6;
    // 6 least significants bits
	int len = cc708ctx->current_packet[0] & 0x3F;

	if (cc708ctx->current_packet_length == 0)
		return;

    // This is well defined in EIA-708; no magic.
    //If packet size code is 0 then length is 128
    //packet size calculation algorithm
	if (len == 0)
		len = 128;
	else
		len = len * 2;

        if (((cc708ctx->prev_seq + 1) % 4) != seq) {
            cc708ctx->fsd->channel_dp_708.sequence_continuity = 0;
        }
        cc708ctx->prev_seq = seq;
        
        ++cc708ctx->seqcnt;
        cc708ctx->seq_mask |= 1 << seq;
        
        if (cc708ctx->seqcnt == 4) {
            cc708ctx->seqcnt = 0;
            if (cc708ctx->seq_mask != 0x0F)
                ++cc708ctx->fsd->channel_dp_708.packet_loss;
            cc708ctx->seq_mask = 0;
        }
    // Is this possible?
	if (cc708ctx->current_packet_length != len) {
		_708_decoders_reset(cc708ctx);
		return;
	}

//	if (cc708ctx->last_sequence != CCX_708_NO_LAST_SEQUENCE &&
//        (cc708ctx->last_sequence + 1) % 4 != seq) {
//		// "[CEA-708] cc_708_process_current_packet: "
//		//"Unexpected sequence number, it is [%d] but should be [%d]\n",
//		//seq, (cc708ctx->last_sequence + 1 ) % 4);
//        ;
//	}

    //cc708ctx->last_sequence = seq;

	pos = cc708ctx->current_packet + 1;

	while (pos < cc708ctx->current_packet + len) {
		// 3 more significant bits
        int service_number = (pos[0] & 0xE0) >> 5;
		// 5 less significant bits
        int block_length = (pos[0] & 0x1F);


		//"[CEA-708] cc_708_process_current_packet: Standard header: "
		//"Service number: [%d] Block length: [%d]\n", service_number, block_length);

		// There is an extended header
        if (service_number == 7) {
			pos++;
            // 6 more significant bits
			service_number = (pos[0] & 0x3F);

			if (service_number < 7) {
				//"[CEA-708] cc_708_process_current_packet: "
				//"Illegal service number in extended header: [%d]\n", service_number);
			}
		}

		pos++; // Move to service data

        // Illegal, but specs say what to do...
		if (service_number == 0 && block_length != 0) {
			//"[CEA-708] ccx_708_process_current_packet: "
			//"Data received for service 0, skipping rest of packet.");
			pos = cc708ctx->current_packet + len; // Move to end
			break;
		}

		if (service_number > 0 ) {
			int service_number_index = service_number - 1;
			cc708ctx->cur_service_number = service_number_index;
			cc708ctx->fsd->svcs_dp_708.service_number[service_number_index] = service_number;
			
			(&cc708ctx->fsd->svcs_dp_708.svc_dps[service_number_index])->svc_type = (SVC_TYPE)service_number;
			cc708ctx->services_active[service_number_index] = 1;
			
			if(cc708ctx->decoders[service_number_index] == NULL){
				cc708ctx->decoders[service_number_index] = (cc_708_service_decoder*)malloc(sizeof(cc_708_service_decoder));

				// init this decoder
				cc_708_service_decoder* decoder = cc708ctx->decoders[service_number_index];
				decoder->cc_count = 0;

				for (int j = 0; j < CC_708_MAX_WINDOWS; j++)
					decoder->windows[j].memory_reserved = 0;

				cc_708_windows_reset(decoder);
			}

			cc_708_process_service_block(cc708ctx,
				cc708ctx->decoders[service_number_index], pos, block_length);

		}
		pos += block_length; // Skip data
	}

	cc_708_clear_packet(cc708ctx);

    // For some reason we didn't parse the whole packet
	if (pos != cc708ctx->current_packet + len) {
		//"[CEA-708] ccx_708_process_current_packet:"
		//" There was a problem with this packet, reseting\n");
        ++cc708ctx->fsd->channel_dp_708.packet_errors;
		_708_decoders_reset(cc708ctx);
	}

    // Null header is mandatory if there is room
	if (len < 128 && *pos) {
		//"[CEA-708] cc_708_process_current_packet: "
		//"Warning: Null header expected but not found.\n");
        ;
	}
}

static void cc_708_process_data(CCaption708SubContext *ctx,
    const unsigned char *data, int data_length) {

    cc_708_ctx *cc708ctx = ctx->cc708ctx;

	if (!cc708ctx->is_active)
		return;

	for (int i = 0; i < data_length; i += 4){
		unsigned char cc_valid = data[i];
		unsigned char cc_type = data[i + 1];

		switch (cc_type) {
			case 2:
				//"[CEA-708] 708_process_data: DTVCC Channel Packet Data";
				if (cc_valid == 0) { // This ends the previous packet
					cc_708_process_current_data(cc708ctx);
                                        ctx->end_of_channel_pkt = 1;
                                                break;
                                }
				//else {
                    if (cc708ctx->current_packet_length > 253) {
                        //"[CEA-708] 708_process_data:
                        //"Warning: Legal packet size exceeded, data not added.\n");
                        ;
					} else {
						cc708ctx->current_packet[cc708ctx->current_packet_length++] = data[i + 2];
						cc708ctx->current_packet[cc708ctx->current_packet_length++] = data[i + 3];
					}
				//}
				break;
			case 3:
				//"[CEA-708] 708_process_data: 708 Channel Packet Start;

                                       cc_708_process_current_data(cc708ctx);

				if (cc_valid) {
					if (cc708ctx->current_packet_length >
                        CCX_708_MAX_PACKET_LENGTH - 1) {

                        //"[CEA-708] 708_process_data: "
						//"Warning: Legal packet size exceeded (2), data not added";
					} else {
						cc708ctx->current_packet[cc708ctx->current_packet_length++] = data[i + 2];
						cc708ctx->current_packet[cc708ctx->current_packet_length++] = data[i + 3];

					}
				}
				break;
			default:
				//"[CEA-708] 708_process_data: "
			    //"shouldn't be here - cc_type: ", cc_type;
                ;
		}
	}
}

static int process_cc_data_pkt(uint8_t *cc_block, CCaption708SubContext *ctx) {

    unsigned char temp[4];
    unsigned char cc_valid;
    unsigned char cc_type;

    cc_valid = (*cc_block & 4) >>2;
    cc_type = (*cc_block) & 3;


    if (cc_valid || cc_type == 3 || cc_type == 2) {

        switch (cc_type) {
            case 0:
                //608 line 21 field 1 cc
                break;
            case 1:
                //608 line 21 field 2 cc
                break;
            case 2: //EIA-708 - 708 packet data
                    // Fall through
                //If start has not been seen then no point moving forward
                if (ctx->start_of_channel_pkt == 0)
                    break;
            case 3: //EIA-708 - 708 start packet data
                temp[0]=cc_valid;
                temp[1]=cc_type;
                temp[2]= cc_block[1];
                temp[3]= cc_block[2];
                ctx->start_of_channel_pkt = 1;
                cc_708_process_data(ctx, (const unsigned char *)temp, 4);
                break;

            default:
             ;
				//Impossible value for cc_type, Please log and move on.

        }
    }
    return 0;
}

static int decode(AVCodecContext *avctx, void *dat, int *size, AVPacket *avpkt) {

    CCaption708SubContext *cc708_ctx = avctx->priv_data;
    AVFrameSideData *fsd = (AVFrameSideData*) dat;
    int ccsize = fsd->size;
    uint8_t *cc_dat = fsd->data;
    int ret = -1;

    init_data_points(fsd);
    //This will be checked against the data packing for setting the cc_count
    //for setting data point
    if (cc708_ctx->expected_cc_count == ccsize/3) {
        unsigned char cc_valid = (*(cc_dat + ccsize -3) & 4) >>2;
        unsigned char cc_type = *(cc_dat + ccsize -3) & 3;
        if (!(cc_valid == 0 && (cc_type == 2 || cc_type ==3)))
            fsd->channel_dp_708.dtvcc_packing_matched = 0;
    } else {
        fsd->channel_dp_708.dtvcc_packing_matched = 0;
    }

    for (int j = 0; j < ccsize; j = j + 3) {
	//if (validate_cc_data_pair((unsigned char*)cc_dat + j))
        //    continue;
        ret = process_cc_data_pkt(cc_dat + j, cc708_ctx);
        if (cc708_ctx->end_of_channel_pkt) {
            cc708_ctx->end_of_channel_pkt = 0;
            break;
        }
    }
    return ret;
}

 static const AVClass ccaption708_dec_class = {
    .class_name = "Closed caption 708 Decoder",
    .item_name  = av_default_item_name,
   // .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_ccaption708_decoder = {
    .name           = "cc_dec708",
    .long_name      = NULL_IF_CONFIG_SMALL("Closed Caption (CEA-708)"),
    .type           = AVMEDIA_TYPE_SUBTITLE,
    .id             = AV_CODEC_ID_EIA_708,
    .priv_data_size = sizeof(CCaption708SubContext),
    .init           = init_decoder,
    .close          = close_decoder,
    .flush          = flush_decoder,
    .decode         = decode,
    .priv_class     = &ccaption708_dec_class,
};
