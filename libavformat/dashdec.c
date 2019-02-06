/*
 * Dynamic Adaptive Streaming over HTTP demux
 * Copyright (c) 2017 samsamsam@o2.pl based on HLS demux
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
 
 /* Code prepared for ffmpeg 2.8.9 and then ported to ffmpeg 3.2.2
  * At now it allow to play one selected representation for audio and video components.
  * 
  */

// #define PRINTING // Only for temporary printfs rest should all be av_log
#define HTTPS // If Defined we replace https in BaseURL with http @Ahmed


/**
 * @file
 */

#include "libavutil/avstring.h"
#include "libavutil/avassert.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/dict.h"
#include "libavutil/time.h"
#include "avformat.h"
#include "internal.h"
#include "avio_internal.h"
#include "url.h"
#include "id3v2.h"

#define INITIAL_BUFFER_SIZE 32768
#define MAX_FIELD_LEN 64

// Defines to assist in printing in different colors. 
// Note: Important to have %s where you want to initiate the color change
// Example: printf( "%sHello, Shahzad\n", blue_str );
#define normal_str  "\x1B[0m"
#define red_str  "\x1B[31m"
#define green_str  "\x1B[32m"
#define yellow_str  "\x1B[33m"
#define blue_str  "\x1B[34m"
#define mag_str  "\x1B[35m"
#define cyan_str  "\x1B[36m"
#define white_str  "\x1B[37m"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#include <time.h>
#include <unistd.h>
#include <string.h>

static struct tm* ResolveUTCDateTime(const char *dateTimeString)
{
    time_t rawtime;
    struct tm* timeInfo = NULL;
    int y = 0;
    int M = 0;
    int d = 0;
    int h = 0;
    int m = 0;
    float s = 0.0;
    
    /* ISO-8601 date parser */
    
    if (dateTimeString == NULL || '\0' == dateTimeString[0])
        return NULL;

    time ( &rawtime );
    timeInfo = gmtime ( &rawtime );

    sscanf(dateTimeString, "%d-%d-%dT%d:%d:%fZ", &y, &M, &d, &h, &m, &s);

    timeInfo->tm_year = y - 1900;
    timeInfo->tm_mon  = M - 1;
    timeInfo->tm_mday = d;

    timeInfo->tm_hour = h;
    timeInfo->tm_min  = m;
    timeInfo->tm_sec  = (int)s;

    return timeInfo;
}

static struct tm*  GetCurrentUTCTime(void)
{
    time_t rawTime;
    time(&rawTime);
    return gmtime(&rawTime);
}

static uint32_t GetCurrentTimeInSec(void)
{
    return (uint32_t)mktime(GetCurrentUTCTime());
}

static uint32_t GetUTCDateTimeInSec(const char *datetime)
{
    return (uint32_t)mktime(ResolveUTCDateTime(datetime));
}

static uint32_t GetDurationInSec(const char *duration)
{
    /* ISO-8601 duration parser */
    
    uint32_t days = 0;
    uint32_t hours = 0;
    uint32_t mins = 0;
    uint32_t secs = 0;

    const char *ptr = duration;
    while(*ptr)
    {
        float value = 0;
        uint32_t charsRead;
        char type = '\0';
        
        if(*ptr == 'P' || *ptr == 'T')
        {
            ptr++;
            continue;
        }

        if(sscanf(ptr, "%f%c%n", &value, &type, &charsRead) != 2) {
            return 0; /* parser error */
        }
        switch(type) {
            case 'D':
                days = (uint32_t)value;
            break;
            case 'H':
                hours = (uint32_t)value;
            break;
            case 'M':
                mins = (uint32_t)value;
            break;
            case 'S':
                secs = (uint32_t)value;
            break;
            default:
            // handle invalid type
                break;
        }
        ptr += charsRead;
    }
    return  ((days * 24 + hours) * 60 + mins) * 60 + secs;
}
//###########################################################################

struct segment {
    //int64_t duration;
    int64_t url_offset;
    int64_t size;
    char *url;
};

struct timeline {
    int64_t t;
    int32_t r;
    int64_t d;
};

enum RepType {
    REP_TYPE_UNSPECIFIED,
    REP_TYPE_AUDIO,
    REP_TYPE_VIDEO
};

enum TemUrlType {
    TMP_URL_TYPE_UNSPECIFIED,
    TMP_URL_TYPE_NUMBER,
    TMP_URL_TYPE_TIME
};

/*
 * Each playlist has its own demuxer. If it currently is active,
 * it has an open AVIOContext too, and potentially an AVPacket
 * containing the next packet from this stream.
 */
struct representation {
    char *url_template;
    char *url_template_pattern;
    char *url_template_format;
    enum TemUrlType tmp_url_type;
    AVIOContext pb;
    AVIOContext *input;
    AVFormatContext *parent;
    AVFormatContext *ctx;
    AVPacket pkt;
    int rep_idx;
    int rep_count;
    int stream_index;

    enum RepType type;
    int64_t target_duration;
    
    int n_segments;
    struct segment **segments; /* VOD list of segment for profile */
    
    int n_timelines;
    struct timeline **timelines;

    int64_t first_seq_no;
    int64_t last_seq_no;
    int64_t start_number; /* used in case when we have dynamic list of segment to know which segments are new one*/
    
    int64_t segmentDuration;
    int64_t segmentTimescalce;
    int64_t presentationTimeOffset;
    
    int64_t cur_seq_no;
    int64_t cur_seg_offset;
    int64_t cur_seg_size;
    struct segment *cur_seg;

    /* Currently active Media Initialization Section */
    struct segment *init_section;
    uint8_t *init_sec_buf;
    uint32_t init_sec_buf_size;
    uint32_t init_sec_data_len;
    uint32_t init_sec_buf_read_offset;
    
    int fix_multiple_stsd_order;

    int64_t cur_timestamp;

    int is_restart_needed;

    /**
     *  record the sequence number of the first segment
     *  in current timeline. 
     *  Since the problem mpd availabilityStartTime is UTC epoch 
     *  starting time, the cur_seq_no is already big number
     *  get_fragment_start_time use local counter to compare 
     *  with cur_seq_no which then blow up the result, it always 
     *  return the last segment timestamp in the timeline.
     *  Use this temporary variable to track the first seq_no.
     */ 

    int64_t first_seq_no_in_representation;

    char id[MAX_FIELD_LEN];
    char codecs[MAX_FIELD_LEN];
    int height;
    int width;
    int frameRate;
    char scanType[MAX_FIELD_LEN];
    char mimeType[MAX_FIELD_LEN];
    char contentType[MAX_FIELD_LEN];
    int bandwidth;

    int needed;

    ffurl_read_callback mpegts_parser_input_backup;
    void* mpegts_parser_input_context_backup;

};

typedef struct DASHContext {
    AVClass *class;

    char *base_url;

    int nb_video_representations;
    int nb_audio_representations;
    int nb_representations;
    struct representation **representations;
    struct representation *cur_video;
    struct representation *cur_audio;
    
    uint32_t mediaPresentationDurationSec;
    
    uint32_t suggestedPresentationDelaySec;
    uint32_t presentationDelaySec;
    uint32_t availabilityStartTimeSec;
    uint32_t publishTimeSec;
    uint32_t minimumUpdatePeriodSec;
    uint32_t timeShiftBufferDepthSec;
    uint32_t minBufferTimeSec;    
    uint32_t periodDurationSec;
    uint32_t periodStartSec;
    uint32_t maxSegmentDuration;
    int is_live;
       
    int audio_rep_index;
    int video_rep_index;

    char *video_rep_id;
    char *audio_rep_id;

    int live_start_index;
    
    AVIOInterruptCB *interrupt_callback;
    char *user_agent;                    ///< holds HTTP user agent set as an AVOption to the HTTP protocol context
    char *cookies;                       ///< holds HTTP cookie values set in either the initial response or as an AVOption to the HTTP protocol context
    char *headers;                       ///< holds HTTP headers set as an AVOption to the HTTP protocol context
    AVDictionary *avio_opts;
    int rep_index;
    char *selected_reps;
} DASHContext;


// ASK AHMED
// checks if representation is among selected
static int is_rep_selected(DASHContext *c, int rep_idx) {
    if ((c->selected_reps != NULL) && (c->selected_reps[0] == '\0')) {// Default value is empty so all representations are selected

        av_log(NULL, AV_LOG_WARNING, "No Representations Selected \n");

        return 1;
    }

    char *str = malloc(sizeof(char) * 254);
    strncpy(str, c->selected_reps, strlen(c->selected_reps));
    char *pt;
    pt = strtok (str,",");

    while (pt != NULL) {
        int a = atoi(pt);

        if (a == rep_idx)
            return 1;
        pt = strtok (NULL, ",");
    }
    return 0;
}


// prints the representation structure in green. @Shahzad for help!
static void print_rep_struct( struct representation *v ) {
    // For testing
    printf( "\n\n%sstruct representation *v {\n", green_str );

    printf( "%s    char *url_template = %s,\n", green_str, v->url_template );
    printf( "%s    AVIOContext *input = %d,\n", green_str, ( (v->input)? 1 : 0 ) );
    printf( "%s    enum RepType type; = %d,\n", green_str, v->tmp_url_type );
    printf( "%s    int rep_idx = %d,\n", green_str, v->rep_idx );
    printf( "%s    int rep_count = %d,\n", green_str, v->rep_count );
    printf( "%s    int stream_index = %d,\n", green_str, v->stream_index );
    printf( "%s    enum AVMediaType type = %d,\n", green_str, v->type );
    printf( "%s    int64_t target_duration = %"PRId64",\n", green_str, v->target_duration );
    printf( "%s    int n_segments = %d,\n", green_str, v->n_segments );
    printf( "%s    int n_timelines = %d,\n", green_str, v->n_timelines );
    //printf("%s    int64_t first_seq_no_in_representation = %d,\n", green_str, v->first_seq_no_in_representation );
    printf( "%s    int64_t first_seq_no = %"PRId64",\n", green_str, v->first_seq_no );
    printf( "%s    int64_t last_seq_no = %"PRId64",\n", green_str, v->last_seq_no );
    printf( "%s    int64_t segmentDuration = %"PRId64",\n", green_str, v->segmentDuration );
    printf( "%s    int64_t segmentTimescale = %"PRId64",\n", green_str, v->segmentTimescalce );
    printf( "%s    int64_t cur_seq_no = %"PRId64",\n", green_str, v->cur_seq_no );
    printf( "%s    int64_t cur_seg_offset = %"PRId64",\n", green_str, v->cur_seg_offset );
    printf( "%s    int64_t cur_seg_size = %"PRId64",\n", green_str, v->cur_seg_size );
    printf( "%s    uint32_t init_sec_buf_size = %d,\n", green_str, v->init_sec_buf_size );
    printf( "%s    uint32_t init_sec_data_len = %d,\n", green_str, v->init_sec_data_len );
    printf( "%s    uint32_t init_sec_buf_read_offset = %d,\n", green_str, v->init_sec_buf_read_offset );
    printf( "%s    int fix_multiple_stsd_order = %d,\n", green_str, v->fix_multiple_stsd_order );
    printf( "%s    int64_t cur_timestamp = %"PRId64"\n", green_str, v->cur_timestamp );

    //if ( (v->cur_seg_size) != -1 ) 
    { // Print the cur_seg within
        printf( "\n\n%s    struct segment *cur_seg; {\n", green_str );

        printf( "%s        int64_t url_offset = %s,\n", green_str, v->url_template );
        printf( "%s        int64_t size = %d,\n", green_str, v->tmp_url_type );
        printf( "%s        char *url = %d,\n", green_str, v->rep_idx );

        printf( "%s    }\n", green_str );
    }

    printf( "%s}\n\n", green_str );
    // End for testing prints
}


static void free_segment(struct segment **seg)
{
    if (!seg || !(*seg)) {
        return;
    }
    
    av_freep(&(*seg)->url);
    av_freep(seg);
}

static void free_segment_list(struct representation *pls)
{
    for (int i = 0; i < pls->n_segments; ++i) {
        av_freep(&pls->segments[i]->url);
        av_freep(&pls->segments[i]);
    }
    av_freep(&pls->segments);
    pls->n_segments = 0;
}

static void free_timelines_list(struct representation *pls)
{
    for (int i = 0; i < pls->n_timelines; ++i) {
        av_freep(&pls->timelines[i]);
    }
    av_freep(&pls->timelines);
    pls->n_timelines = 0;
}

/*
 * Used to reset a statically allocated AVPacket to a clean slate,
 * containing no data.
 */
static void reset_packet(AVPacket *pkt)
{
    av_init_packet(pkt);
    pkt->data = NULL;
}

static void free_representation(struct representation *pls)
{
    free_segment_list(pls);
    free_timelines_list(pls);
    free_segment(&pls->cur_seg);
    free_segment(&pls->init_section);
    av_freep(&pls->init_sec_buf);
    av_packet_unref(&pls->pkt);
    reset_packet(&pls->pkt);
    av_freep(&pls->pb.buffer);
    if (pls->input)
        ff_format_io_close(pls->parent, &pls->input);
    if (pls->ctx) {
        pls->ctx->pb = NULL;
        avformat_close_input(&pls->ctx);
    }
    
    av_free(pls->url_template_pattern);
    av_free(pls->url_template_format);
    av_free(pls->url_template);
    av_free(pls);
}

static void update_options(char **dest, const char *name, void *src)
{
    av_freep(dest);
    av_opt_get(src, name, AV_OPT_SEARCH_CHILDREN, (uint8_t**)dest);
    if (*dest && !strlen(*dest))
        av_freep(dest);
}

static int open_url(AVFormatContext *s, AVIOContext **pb, const char *url,
                    AVDictionary *opts, AVDictionary *opts2, int *is_http)
{

    #ifdef PRINTING
    printf("================\n");
    printf("OPENING URL: %s\n", url);
    printf("================\n");
    #endif // PRINTING

    DASHContext *c = s->priv_data;
    AVDictionary *tmp = NULL;
    const char *proto_name = NULL;
    int ret;

    av_dict_copy(&tmp, opts, 0);
    av_dict_copy(&tmp, opts2, 0);

    if (!proto_name)
        proto_name = avio_find_protocol_name(url);

    if (!proto_name)
        return AVERROR_INVALIDDATA;

    // only http(s) & file are allowed
    if (!av_strstart(proto_name, "http", NULL) && !av_strstart(proto_name, "file", NULL))
        return AVERROR_INVALIDDATA;
    if (!strncmp(proto_name, url, strlen(proto_name)) && url[strlen(proto_name)] == ':')
        ;
    else if (strcmp(proto_name, "file") || !strncmp(url, "file,", 5))
        return AVERROR_INVALIDDATA;

    #ifdef PRINTING
    struct timespec begin, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &begin);
    ret = s->io_open(s, pb, url, AVIO_FLAG_READ, &tmp);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    uint64_t microSec = (end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_nsec - begin.tv_nsec) / 1000;
    printf( "%sio_open in open_url took %d micro-seconds to complete.\n%s", red_str, microSec, normal_str );
    #else
    ret = s->io_open(s, pb, url, AVIO_FLAG_READ, &tmp);
    #endif 

    if (ret >= 0) {
        // update cookies on http response with setcookies.
        void *u = (s->flags & AVFMT_FLAG_CUSTOM_IO) ? NULL : s->pb;
        update_options(&c->cookies, "cookies", u);
        av_dict_set(&opts, "cookies", c->cookies, 0);
    }

    av_dict_free(&tmp);

    if (is_http)
        *is_http = av_strstart(proto_name, "http", NULL);

    return ret;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#if (__STDC_VERSION__ >= 199901L)
#include <stdint.h>
#endif

//http://creativeandcritical.net/str-replace-c

static char *repl_str(const char *str, const char *from, const char *to)
{
    /* Adjust each of the below values to suit your needs. */

    /* Increment positions cache size initially by this number. */
    size_t cache_sz_inc = 16;
    /* Thereafter, each time capacity needs to be increased,
     * multiply the increment by this factor. */
    const size_t cache_sz_inc_factor = 3;
    /* But never increment capacity by more than this number. */
    const size_t cache_sz_inc_max = 1048576;

    char *pret, *ret = NULL;
    const char *pstr2, *pstr = str;
    size_t count = 0;
    #if (__STDC_VERSION__ >= 199901L)
    uintptr_t *pos_cache_tmp, *pos_cache = NULL;
    #else
    ptrdiff_t *pos_cache_tmp, *pos_cache = NULL;
    #endif
    size_t cache_sz = 0;
    size_t cpylen, orglen, retlen, tolen, fromlen = strlen(from);

    /* Find all matches and cache their positions. */
    while ((pstr2 = strstr(pstr, from)) != NULL) {
        ++count;

        /* Increase the cache size when necessary. */
        if (cache_sz < count) {
            cache_sz += cache_sz_inc;
            pos_cache_tmp = realloc(pos_cache, sizeof(*pos_cache) * cache_sz);
            if (pos_cache_tmp == NULL) {
                goto end_repl_str;
            } else pos_cache = pos_cache_tmp;
            cache_sz_inc *= cache_sz_inc_factor;
            if (cache_sz_inc > cache_sz_inc_max) {
                cache_sz_inc = cache_sz_inc_max;
            }
        }

        pos_cache[count-1] = pstr2 - str;
        pstr = pstr2 + fromlen;
    }

    orglen = pstr - str + strlen(pstr);

    /* Allocate memory for the post-replacement string. */
    if (count > 0) {
        tolen = strlen(to);
        retlen = orglen + (tolen - fromlen) * count;
    } else retlen = orglen;
    ret = av_malloc(retlen + 1);
    if (ret == NULL) {
        goto end_repl_str;
    }

    if (count == 0) {
        /* If no matches, then just duplicate the string. */
        strcpy(ret, str);
    } else {
        /* Otherwise, duplicate the string whilst performing
         * the replacements using the position cache. */
        pret = ret;
        memcpy(pret, str, pos_cache[0]);
        pret += pos_cache[0];
        for ( size_t i = 0; i < count; ++i ) {
            memcpy(pret, to, tolen);
            pret += tolen;
            pstr = str + pos_cache[i] + fromlen;
            cpylen = (i == count-1 ? orglen : pos_cache[i+1]) - pos_cache[i] - fromlen;
            memcpy(pret, pstr, cpylen);
            pret += cpylen;
        }
        ret[retlen] = '\0';
    }

end_repl_str:
    /* Free the cache and return the post-replacement string,
     * which will be NULL in the event of an error. */
    free(pos_cache);
    return ret;
}

static int get_repl_pattern_and_format(const char *i_url, const char *i_marker, char **o_pattern, char **o_format)
{
    int ret = -1;
    
    if (NULL != strstr(i_url, i_marker)) {
        *o_pattern = av_strdup(i_marker);
        *o_format = av_strdup("%"PRId64);
        ret = 0;
    } else {
        char *prefix = NULL;
        char *start  = NULL;
        char *end    = NULL;
        int marker_len = 0;
        int format_len = 0;
        
        prefix = av_strdup(i_marker);
        marker_len = strlen(prefix)-1;
        prefix[marker_len] = '\0';
        
        start = strstr(i_url, prefix);
        if (!start)
            goto finish;
        end = strchr(start+1, '$');
        if (!end)
            goto finish;

        if (start[marker_len] != '%')
            goto finish;

        if (end[-1] != 'd')
            goto finish;
        
        format_len = end - start - marker_len - 1 + strlen(PRId64);
        *o_format = av_mallocz(format_len+1);
        strncpy(*o_format, start + marker_len, end - start - marker_len -1);
        strcat(*o_format, PRId64);
        
        *o_pattern = av_mallocz(end - start + 2);
        strncpy(*o_pattern, start, end - start + 1);
        
        ret = 0;
finish:
        free(prefix);
    }
    
    return ret;
}


#ifdef HTTPS // @Ahmed
static void delete_char(char *str, int i) {
    int len = strlen(str);

    for (; i < len - 1 ; ++i)
    {
       str[i] = str[i+1];
    }

    str[i] = '\0';
}
#endif //HTTPS


static char * get_content_url(xmlNodePtr *baseUrlNodes, int n_baseUrlNodes, xmlChar *rep_id_val, xmlChar *rep_bandwidth_val, xmlChar *val)
{
    char *tmp_str = av_mallocz(MAX_URL_SIZE);
    char *url = NULL;

    for (int i = 0; i < n_baseUrlNodes; ++i) {

        if (baseUrlNodes[i] && baseUrlNodes[i]->children && baseUrlNodes[i]->children->type == XML_TEXT_NODE) {
            xmlChar *text = xmlNodeGetContent(baseUrlNodes[i]->children);
            if (text) {
                char *tmp_str_2 = av_mallocz(MAX_URL_SIZE);
                ff_make_absolute_url(tmp_str_2, MAX_URL_SIZE, tmp_str, text);

                #ifdef HTTPS
                if (strstr(tmp_str_2, "https") != NULL) { delete_char( tmp_str_2, 4 ); }
                #endif //HTTPS

                av_free(tmp_str);
                tmp_str = tmp_str_2;
                xmlFree(text);
            }
        }
    }
    
    url = tmp_str;
    
    if (val)
        strcat(tmp_str, (const char*)val);
        
    if (rep_id_val) {
        url = repl_str(tmp_str, "$RepresentationID$", (const char*)rep_id_val);
        av_free(tmp_str);
        tmp_str = url;
    }
        
    if (rep_bandwidth_val && tmp_str){
        char *pFormat = NULL;
        char *pPattern = NULL;
        
        if (0 == get_repl_pattern_and_format(tmp_str, "$Bandwidth$", &pPattern, &pFormat)) {
            int64_t val = (int64_t)atoll((const char *)rep_bandwidth_val);
            int size = snprintf(NULL, 0, pFormat, val); // calc needed buffer size
            if (val > 0) {
                char *tmp_val = av_mallocz(size + 1);
                snprintf(tmp_val, size+1, pFormat, val);
                
                url = repl_str(tmp_str, pPattern, tmp_val);
                av_free(tmp_val);
                
                av_free(tmp_str);
                tmp_str = NULL;
            }
            av_free(pFormat);
            av_free(pPattern);
        }
    }
    
    if (tmp_str != url)
        av_free(tmp_str);
    
    return url;
}

static xmlChar * get_val_from_nodes_tab(xmlNodePtr *nodes, const int n_nodes, const xmlChar *attrName)
{
    for (int i = 0; i < n_nodes; ++i) {
        if (nodes[i]) {
            xmlChar *val = xmlGetProp(nodes[i], attrName);
            if (val)
                return val;
        }
    }
    return NULL;
}

static xmlNodePtr findChildNodeByName(xmlNodePtr rootnode, const xmlChar *nodename)
{
    xmlNodePtr node = rootnode;
    if (node == NULL) {
        return NULL;
    }

    node = xmlFirstElementChild(node);
    while (node != NULL) {

        if (!xmlStrcmp(node->name, nodename)) {
            return node; 
        }
        node = xmlNextElementSibling(node);
    }
    return NULL;
}

static enum RepType get_content_type(xmlNodePtr node, xmlChar **mimeType, xmlChar **contentType) 
{
    enum RepType type = REP_TYPE_UNSPECIFIED;

    xmlChar *val = NULL;

    if (node) {
        
        const char *attr = "contentType";
        val = xmlGetProp(node, attr);
        if (val) {
            *contentType = val;
            if (strstr((const char *) val, "video"))
                type = REP_TYPE_VIDEO;
            else if (strstr((const char *) val, "audio"))
                type = REP_TYPE_AUDIO;
        }

        attr = "mimeType"; 
        val = xmlGetProp(node, attr);
        if (type == REP_TYPE_UNSPECIFIED) {
            if (val) {
                *mimeType = val;
                if (strstr((const char *) val, "video"))
                    type = REP_TYPE_VIDEO;
                else if (strstr((const char *) val, "audio"))
                    type = REP_TYPE_AUDIO;
            }
        }

        #ifdef OLD
        //for ( int i = 0; ( (type == REP_TYPE_UNSPECIFIED) && (i < 2) ); ++i ) {    
        for ( int i = 0; i < 2; ++i ) {
            
            const char *attr = (i == 0) ? "contentType" : "mimeType"; 
            
            //xmlChar *val = xmlGetProp(node, attr);
            val = xmlGetProp(node, attr);

            if (val) {

                if (strstr((const char *) val, "video")) {
                    type = REP_TYPE_VIDEO;
                    if (i == 0) { *contentType = val; }
                    if (i == 1) { *mimeType = val; }
                } else if (strstr((const char *) val, "audio")) {
                    type = REP_TYPE_AUDIO;
                    if (i == 0) { *contentType = val; }
                    if (i == 1) { *mimeType = val; }

                }
                //xmlFree(val); ASK AHMED WHY COMMENTED
            }

        } // End of For-Loop
        #endif
    
    }

    return( type );
}


static void fill_timelines(struct representation *rep, xmlNodePtr *nodes, const int n_nodes)
{
    for ( int i = 0; i < n_nodes; ++i) {
        if (nodes[i]) {

            xmlNodePtr segmentTimelineNode = findChildNodeByName(nodes[i], "SegmentTimeline");
            if (segmentTimelineNode) {

                segmentTimelineNode = xmlFirstElementChild(segmentTimelineNode);
                while (segmentTimelineNode) {

                    if (!xmlStrcmp(segmentTimelineNode->name, (const xmlChar *)"S")) {

                        struct timeline *tml = av_mallocz(sizeof(struct timeline));
                        xmlAttrPtr attr = segmentTimelineNode->properties;
                        while(attr) {

                            xmlChar *val = xmlGetProp(segmentTimelineNode, attr->name);
                            
                            if (!xmlStrcmp(attr->name, (const xmlChar *)"t"))
                                tml->t = (int64_t)atoll((const char *)val);
                            else if (!xmlStrcmp(attr->name, (const xmlChar *)"r"))
                                tml->r =(int32_t) atoi((const char *)val);
                            else if (!xmlStrcmp(attr->name, (const xmlChar *)"d"))
                                tml->d = (int64_t)atoll((const char *)val);
                            attr = attr->next;
                            xmlFree(val);
                        }
                        
                        dynarray_add(&rep->timelines, &rep->n_timelines, tml);
                    }
                    segmentTimelineNode = xmlNextElementSibling(segmentTimelineNode);
                }
                return;
            }
        }
    }
    return;
}


static int parse_mainifest(AVFormatContext *s, const char *url, AVIOContext *in)
{
    DASHContext *c = s->priv_data;

    int repIndex = c->rep_index;

    av_log(NULL, AV_LOG_VERBOSE, "(repIndex, rep_index) = (%d, %d)\n", repIndex, c->rep_index);

    int ret = 0;
    int close_in = 0;
    uint8_t *new_url = NULL;
    
    int64_t filesize = 0;
    char *buffer = NULL;

    if (!in) {
        AVDictionary *opts = NULL;
        close_in = 1;
        /* This is XML mainfest there is no need to set range header */
        av_dict_set(&opts, "seekable", "0", 0);

        // broker prior HTTP options that should be consistent across requests
        av_dict_set(&opts, "user-agent", c->user_agent, 0);
        av_dict_set(&opts, "cookies", c->cookies, 0);
        av_dict_set(&opts, "headers", c->headers, 0);

        ret = avio_open2(&in, url, AVIO_FLAG_READ,
                         c->interrupt_callback, &opts);
        av_dict_free(&opts);
        if (ret < 0)
            return ret;
    }

    if (av_opt_get(in, "location", AV_OPT_SEARCH_CHILDREN, &new_url) >= 0) {
        c->base_url = av_strdup(new_url);
    } else {
        c->base_url = av_strdup(url);
    }
    
    filesize = avio_size(in);
    if (filesize <= 0){
        filesize = 8 * 1024;
    }
      
    buffer = av_mallocz(filesize);
    
    if (!buffer) {
        return AVERROR(ENOMEM);
    }
    
    filesize = avio_read(in, buffer, filesize);
    if (filesize > 0) {
        xmlDoc *doc = NULL;
        xmlNodePtr root_element = NULL;
        xmlNodePtr node = NULL;
        xmlNodePtr periodNode = NULL;
        xmlNodePtr mpdBaseUrlNode = NULL;
        xmlNodePtr periodBaseUrlNode = NULL;
        xmlNodePtr adaptionSetNode = NULL;
        xmlAttrPtr attr = NULL;
        xmlChar *val  = NULL;
        uint32_t perdiodDurationSec = 0;
        uint32_t perdiodStartSec = 0;
        
        int32_t audioRepIdx = 0;
        int32_t videoRepIdx = 0;
        
        LIBXML_TEST_VERSION
    
        doc = xmlReadMemory(buffer, filesize, c->base_url, NULL, 0);
        root_element = xmlDocGetRootElement(doc);
        node = root_element;
        
        if (!node) {
            ret = AVERROR_INVALIDDATA;
            av_log(s, AV_LOG_ERROR, "Unable to parse '%s' - missing root node\n", url);
            goto cleanup;
        }
        
        if (node->type != XML_ELEMENT_NODE || 
            xmlStrcmp(node->name, (const xmlChar *)"MPD")) {
            ret = AVERROR_INVALIDDATA;
            av_log(s, AV_LOG_ERROR, "Unable to parse '%s' - wrong root node name[%s] type[%d]\n", url, node->name, (int)node->type);
            goto cleanup;
        }
        
        val = xmlGetProp(node, "type");
        if (!val) {
            av_log(s, AV_LOG_ERROR, "Unable to parse '%s' - missing type attrib\n", url);
            ret = AVERROR_INVALIDDATA;
            goto cleanup;
        }
        if (!xmlStrcmp(val, (const xmlChar *)"dynamic"))
            c->is_live = 1;
        xmlFree(val);
        
        attr = node->properties;
        while(attr) {
            val = xmlGetProp(node, attr->name);
            
            if (!xmlStrcmp(attr->name, (const xmlChar *)"availabilityStartTime"))
                c->availabilityStartTimeSec = GetUTCDateTimeInSec((const char *)val);
            else if (!xmlStrcmp(attr->name, (const xmlChar *)"publishTime"))
                c->publishTimeSec = GetUTCDateTimeInSec((const char *)val);
            else if (!xmlStrcmp(attr->name, (const xmlChar *)"minimumUpdatePeriod"))
                c->minimumUpdatePeriodSec = GetDurationInSec((const char *)val);
            else if (!xmlStrcmp(attr->name, (const xmlChar *)"timeShiftBufferDepth"))
                c->timeShiftBufferDepthSec = GetDurationInSec((const char *)val);
            else if (!xmlStrcmp(attr->name, (const xmlChar *)"minBufferTime"))
                c->minBufferTimeSec = GetDurationInSec((const char *)val);
            else if (!xmlStrcmp(attr->name, (const xmlChar *)"suggestedPresentationDelay"))
                c->suggestedPresentationDelaySec = GetDurationInSec((const char *)val);
            else if (!xmlStrcmp(attr->name, (const xmlChar *)"mediaPresentationDuration"))
                c->mediaPresentationDurationSec = GetDurationInSec((const char *)val);
            else if (!xmlStrcmp(attr->name, (const xmlChar *)"maxSegmentDuration"))
                c->maxSegmentDuration = GetDurationInSec((const char *)val);
            attr = attr->next;
            
            xmlFree(val);
        } 
        
        mpdBaseUrlNode = findChildNodeByName(node, "BaseURL");
        
        // at now we can handle only one period, with the longest duration
        node = xmlFirstElementChild(node);
        int nb_periods = 0;
        while (node) {
            if (!xmlStrcmp(node->name, (const xmlChar *)"Period")) {

                av_log(NULL, AV_LOG_INFO, "Period: [%d]\n", ++nb_periods);

                perdiodDurationSec = 0;
                perdiodStartSec = 0;

                attr = node->properties;
                while(attr) {
                    val = xmlGetProp(node, attr->name);
                    
                    if (!xmlStrcmp(attr->name, (const xmlChar *)"duration"))
                        perdiodDurationSec = GetDurationInSec((const char *)val);
                    else if (!xmlStrcmp(attr->name, (const xmlChar *)"start"))
                        perdiodStartSec = GetDurationInSec((const char *)val);
                    attr = attr->next;
                    
                    xmlFree(val);
                }
                
                if ((perdiodDurationSec) >= (c->periodDurationSec)) {
                    periodNode = node;
                    c->periodDurationSec = perdiodDurationSec;
                    c->periodStartSec = perdiodStartSec;
                    if (c->periodStartSec > 0)
                        c->mediaPresentationDurationSec = c->periodDurationSec;
                }
            }
            
            node = xmlNextElementSibling(node);
        }
        
        if (!periodNode) {
            av_log(s, AV_LOG_ERROR, "Unable to parse '%s' - missing Period node\n", url);
            ret = AVERROR_INVALIDDATA;
            goto cleanup;
        }
       
        // explore AdaptationSet
        adaptionSetNode = xmlFirstElementChild(periodNode);
        int nb_adaptationsets = 0;

        while (adaptionSetNode) {
            if (!xmlStrcmp(adaptionSetNode->name, (const xmlChar *)"BaseURL")) {
                periodBaseUrlNode = adaptionSetNode;
            } else if (!xmlStrcmp(adaptionSetNode->name, (const xmlChar *)"AdaptationSet")) {

                av_log(NULL, AV_LOG_INFO, "Adaptation Set: [%d] \n", ++nb_adaptationsets);

                xmlNodePtr segmentTemplateNode = NULL;
                xmlNodePtr contentComponentNode = NULL;
                xmlNodePtr adaptionSetBaseUrlNode = NULL;
                xmlNodePtr adaptionSetSegmentListNode = NULL;
                
                node = xmlFirstElementChild(adaptionSetNode);

                int nb_representation = 0;
                while (node) {
                    if (!xmlStrcmp(node->name, (const xmlChar *)"SegmentTemplate")) {
                        segmentTemplateNode = node;
                    } else if (!xmlStrcmp(node->name, (const xmlChar *)"ContentComponent")) {
                        contentComponentNode = node;
                    } else if (!xmlStrcmp(node->name, (const xmlChar *)"BaseURL")) {
                        adaptionSetBaseUrlNode = node;
                    } else if (!xmlStrcmp(node->name, (const xmlChar *)"SegmentList")) {
                        adaptionSetSegmentListNode = node;
                    } else if (!xmlStrcmp(node->name, (const xmlChar *)"Representation")) {

                        xmlNodePtr representationNode = node;
                        
                        xmlChar *rep_id_val = xmlGetProp(representationNode, "id");
                        xmlChar *rep_bandwidth_val = xmlGetProp(representationNode, "bandwidth");
                        //xmlChar *rep_mimeType_val = xmlGetProp(representation_node, "mimeType");
                        xmlChar *rep_mimeType_val = NULL;
                        xmlChar *rep_contentType_val = NULL;
                        xmlChar *rep_codecs_val = xmlGetProp(representationNode, "codecs");
                        xmlChar *rep_height_val = xmlGetProp(representationNode, "height");
                        xmlChar *rep_width_val = xmlGetProp(representationNode, "width");
                        xmlChar *rep_frameRate_val = xmlGetProp(representationNode, "frameRate");
                        xmlChar *rep_scanType_val = xmlGetProp(representationNode, "scanType");

                        enum RepType type = REP_TYPE_UNSPECIFIED;
                        
                        
                        // try get information from representation
                        if (type == REP_TYPE_UNSPECIFIED) {
                            type = get_content_type(representationNode, &rep_mimeType_val, &rep_contentType_val);
                        }
                        // try get information from adaption set
                        if (type == REP_TYPE_UNSPECIFIED) {
                            type = get_content_type(adaptionSetNode, &rep_mimeType_val, &rep_contentType_val);
                        }
                        // try get information from contentComponen
                        if (type == REP_TYPE_UNSPECIFIED) {
                            type = get_content_type(contentComponentNode, &rep_mimeType_val, &rep_contentType_val);
                        }
                        
                        
                        if (type == REP_TYPE_UNSPECIFIED) {
                            av_log(s, AV_LOG_VERBOSE, "Parsing [%s] : SKIPPING because representation of this type is not supported \n", url);
                        } 

                        //else if ( (type == REP_TYPE_VIDEO && ((c->video_rep_index < 0 && !c->cur_video) || videoRepIdx == (int32_t)c->video_rep_index )) || 
                        //          (type == REP_TYPE_AUDIO && ((c->audio_rep_index < 0 && !c->cur_audio) || audioRepIdx == (int32_t)c->audio_rep_index )) ) { 
                        
                        //else if ( (type == REP_TYPE_VIDEO && ( ( c->video_rep_index < 0 ) || videoRepIdx == (int32_t)c->video_rep_index )) || 
                        //          (type == REP_TYPE_AUDIO && ( ( c->audio_rep_index < 0 ) || audioRepIdx == (int32_t)c->audio_rep_index )) ) {
                        
                        else if ( (type == REP_TYPE_VIDEO && ( ( strcmp(c->video_rep_id, "") == 0 ) || ( strcmp(c->video_rep_id, rep_id_val) == 0 ) )) || 
                                  (type == REP_TYPE_AUDIO && ( ( strcmp(c->audio_rep_id, "") == 0 ) || ( strcmp(c->audio_rep_id, rep_id_val) == 0 ) )) ) {

                            struct representation *rep = av_mallocz(sizeof(struct representation));
                            dynarray_add(&c->representations, &c->nb_representations, rep); // WHY ADDED AND REMOVED FROM OTHER PLACE

                            // Added to read more metadata from manifest and expand Representation structure. @ShahzadLone for info!
                            av_log(NULL, AV_LOG_VERBOSE, "rep(id[%s],mimeType[%s],contentType[%s],codecs[%s],height[%s],width[%s],frameRate[%s],scanType[%s],bandwidth[%s]) -- before \n", 
                                   (char *)rep_id_val, (char *)rep_mimeType_val, (char *)rep_contentType_val, (char *)rep_codecs_val, (char *)rep_height_val, (char *)rep_width_val, (char *)rep_frameRate_val, (char *)rep_scanType_val, (char *)rep_bandwidth_val);
                            
                            if (rep_id_val) { strcpy(rep->id, rep_id_val); }

                            if (rep_mimeType_val) { strcpy(rep->mimeType, rep_mimeType_val); }

                            if (rep_contentType_val) { strcpy(rep->contentType, rep_contentType_val); }

                            if (rep_codecs_val) { strcpy(rep->codecs, rep_codecs_val); }
  
                            rep->height = 0;
                            if (rep_height_val) { rep->height = strtol((char *)rep_height_val, NULL, 0); }

                            rep->width = 0;
                            if (rep_width_val) { rep->width = strtol((char *)rep_width_val, NULL, 0); }
                           
                            rep->frameRate = 0;
                            if (rep_frameRate_val) { rep->frameRate = strtol((char *)rep_frameRate_val, NULL, 0); }
   
                            if (rep_scanType_val) { strcpy(rep->scanType, rep_scanType_val); }

                            rep->bandwidth = 0;
                            if (rep_bandwidth_val) { rep->bandwidth = strtol((char *)rep_bandwidth_val, NULL, 0); }

                            av_log(NULL, AV_LOG_VERBOSE, "rep(id[%s],mimeType[%s],contentType[%s],codecs[%s],height[%d],width[%d],frameRate[%d],scanType[%s],bandwidth[%d]) -- after \n",
                                   rep->id, rep->mimeType, rep->contentType, rep->codecs, rep->height, rep->width, rep->frameRate, rep->scanType, rep->bandwidth);
                            
                            xmlNodePtr representationSegmentTemplateNode = findChildNodeByName(representationNode, "SegmentTemplate");
                            xmlNodePtr representationBaseUrlNode = findChildNodeByName(representationNode, "BaseURL");
                            xmlNodePtr representationSegmentListNode = findChildNodeByName(representationNode, "SegmentList");
                            
                            reset_packet(&rep->pkt);
                            
                            if (representationSegmentTemplateNode || segmentTemplateNode) {
                                
                                xmlNodePtr segmentTemplatesTab[2] = {representationSegmentTemplateNode, segmentTemplateNode};
                                xmlChar *duration_val        = get_val_from_nodes_tab(segmentTemplatesTab,  2, "duration");
                                xmlChar *startNumber_val     = get_val_from_nodes_tab(segmentTemplatesTab,  2, "startNumber");
                                xmlChar *timescale_val       = get_val_from_nodes_tab(segmentTemplatesTab,  2, "timescale");
                                xmlChar *presentationTimeOffset_val       = get_val_from_nodes_tab(segmentTemplatesTab,  2, "presentationTimeOffset");
                                xmlChar *initialization_val  = get_val_from_nodes_tab(segmentTemplatesTab,  2, "initialization");
                                xmlChar *media_val           = get_val_from_nodes_tab(segmentTemplatesTab,  2, "media");
                                
                                xmlNodePtr baseUrlNodes[4] = {mpdBaseUrlNode, periodBaseUrlNode, adaptionSetBaseUrlNode, representationBaseUrlNode};
                                
                                if (initialization_val) {
                                    rep->init_section = av_mallocz(sizeof(struct segment));
                                    rep->init_section->url = get_content_url(baseUrlNodes, 4, rep_id_val, rep_bandwidth_val, initialization_val);
                                    rep->init_section->size = -1;
                                    xmlFree(initialization_val);
                                }
                                
                                if (media_val) {
                                    char *tmp_str = get_content_url(baseUrlNodes, 4, rep_id_val, rep_bandwidth_val, media_val);
                                    rep->tmp_url_type = TMP_URL_TYPE_UNSPECIFIED;
                                    if (tmp_str) {
                                        rep->url_template = tmp_str;
                                        
                                        if ( ( strstr( tmp_str, "$Number" ) ) && 
                                             ( 0 == get_repl_pattern_and_format( tmp_str, "$Number$", &( rep->url_template_pattern ), &( rep->url_template_format ) ) )
                                           ) {
                                            //  A URL template is provided from which clients build a chunk list where the chunk URLs include chunk numbers (like index numbers).
                                            rep->tmp_url_type = TMP_URL_TYPE_NUMBER; // (Chunk) Number-Based.
                                        } else if ( ( strstr( tmp_str, "$Time" ) ) &&
                                                    ( 0 == get_repl_pattern_and_format( tmp_str, "$Time$", &( rep->url_template_pattern ), &( rep->url_template_format ) ) ) 
                                                  ) {
                                            // A URL template is provided from which clients build a chunk list where the chunk URLs include chunk start times.
                                            rep->tmp_url_type = TMP_URL_TYPE_TIME; // (CTime-Based.
                                        } 
                                    }
                                    xmlFree(media_val);
                                }
                                
                                if (duration_val) {
                                    rep->segmentDuration = (int64_t) atoll((const char *)duration_val);
                                    xmlFree(duration_val);
                                }
                                
                                if (timescale_val) {
                                    rep->segmentTimescalce = (int64_t) atoll((const char *)timescale_val);
                                    xmlFree(timescale_val);
                                }
                                
                                if (presentationTimeOffset_val) {
                                    rep->presentationTimeOffset = (int64_t) atoll((const char *)presentationTimeOffset_val);
                                    xmlFree(presentationTimeOffset_val);
                                }
                                
                                if (startNumber_val) {
                                    if (rep->tmp_url_type == TMP_URL_TYPE_NUMBER)
                                        rep->first_seq_no = (int64_t) atoll((const char *)startNumber_val);
                                    else
                                        rep->first_seq_no = 0;
                                    xmlFree(startNumber_val);
                                }
                                
                                fill_timelines(rep, segmentTemplatesTab, 2);

                            } else if (representationBaseUrlNode && !representationSegmentListNode) {
                                xmlNodePtr baseUrlNodes[4] = {mpdBaseUrlNode, periodBaseUrlNode, adaptionSetBaseUrlNode, representationBaseUrlNode};
                                struct segment *seg = av_mallocz(sizeof(struct segment));
                                seg->url = get_content_url(baseUrlNodes, 4, rep_id_val, rep_bandwidth_val, NULL);
                                seg->size = -1;
                                dynarray_add(&rep->segments, &rep->n_segments, seg);
                            } else if (representationSegmentListNode) {
                               // TODO: https://www.brendanlong.com/the-structure-of-an-mpeg-dash-mpd.html
                               // http://www-itec.uni-klu.ac.at/dash/ddash/mpdGenerator.php?segmentlength=15&type=full
                                xmlNodePtr segmentUrlNode = NULL;
                                xmlNodePtr segmentListTab[2] = {representationSegmentListNode, adaptionSetSegmentListNode};
                                xmlChar *duration_val        = get_val_from_nodes_tab(segmentListTab,  2, "duration");
                                xmlChar *startNumber_val     = get_val_from_nodes_tab(segmentListTab,  2, "startNumber");
                                xmlChar *timescale_val       = get_val_from_nodes_tab(segmentListTab,  2, "timescale");
                                
                                if (duration_val) {
                                    rep->segmentDuration = (int64_t) atoll((const char *)duration_val);
                                    xmlFree(duration_val);
                                }
                                
                                if (timescale_val) {
                                    rep->segmentTimescalce = (int64_t) atoll((const char *)timescale_val);
                                    xmlFree(timescale_val);
                                }
                                
                                if (startNumber_val) {
                                    rep->start_number = (int64_t) atoll((const char *)startNumber_val);
                                    xmlFree(startNumber_val);
                                }
                                
                                segmentUrlNode = xmlFirstElementChild(representationSegmentListNode);
                                while (segmentUrlNode) {
                                    if (!xmlStrcmp(segmentUrlNode->name, (const xmlChar *)"Initialization")) {
                                        xmlChar *initialization_val = xmlGetProp(segmentUrlNode, "sourceURL");
                                        if (initialization_val) {
                                            xmlNodePtr baseUrlNodes[4] = {mpdBaseUrlNode, periodBaseUrlNode, adaptionSetBaseUrlNode, representationBaseUrlNode};
                                            rep->init_section = av_mallocz(sizeof(struct segment));
                                            rep->init_section->url = get_content_url(baseUrlNodes, 4, rep_id_val, rep_bandwidth_val, initialization_val);
                                            rep->init_section->size = -1;
                                            xmlFree(initialization_val);
                                        }
                                    } else if (!xmlStrcmp(segmentUrlNode->name, (const xmlChar *)"SegmentURL")) {
                                        xmlChar *media_val = xmlGetProp(segmentUrlNode, "media");
                                        if (media_val) {
                                            xmlNodePtr baseUrlNodes[4] = {mpdBaseUrlNode, periodBaseUrlNode, adaptionSetBaseUrlNode, representationBaseUrlNode};
                                            struct segment *seg = av_mallocz(sizeof(struct segment));
                                            seg->url = get_content_url(baseUrlNodes, 4, rep_id_val, rep_bandwidth_val, media_val);
                                            seg->size = -1;
                                            dynarray_add(&rep->segments, &rep->n_segments, seg);
                                            xmlFree(media_val);
                                        }
                                    }
                                    segmentUrlNode = xmlNextElementSibling(segmentUrlNode);
                                }
                                
                                fill_timelines(rep, segmentListTab, 2);
                            } else {
                                free_representation(rep);
                                rep = NULL;
                                av_log(s, AV_LOG_ERROR, "Unknown format of Representation node id[%s] \n", (const char *)rep_id_val);
                            }
                            
                            if (rep) {
                                if (rep->segmentDuration > 0 && rep->segmentTimescalce == 0)
                                    rep->segmentTimescalce = 1;
                                
                                if (type == REP_TYPE_VIDEO) {

                                    rep->rep_idx = videoRepIdx;
                                    c->cur_video = rep;
          
                                }

                                else { // (type == REP_TYPE_AUDIO)

                                    rep->rep_idx = audioRepIdx;
                                    c->cur_audio = rep;

                                }
                            }
                            av_log(NULL, AV_LOG_INFO, "Representation: Number = %d, Type = %d , ID = %s\n", ++nb_representation, type, (const char *)rep_id_val);
                        }
                        
                        if (type == REP_TYPE_VIDEO) {
                            videoRepIdx += 1;
                        } if (type == REP_TYPE_AUDIO) {
                            audioRepIdx += 1;
                        }
                        
                        if (rep_id_val)
                            xmlFree(rep_id_val);
                        
                        if (rep_bandwidth_val)
                            xmlFree(rep_bandwidth_val);

                    }
                    node = xmlNextElementSibling(node);
                }

            }
            adaptionSetNode = xmlNextElementSibling(adaptionSetNode);
        }
        
        if (c->cur_video) {
            c->cur_video->rep_count = videoRepIdx;
            c->cur_video->fix_multiple_stsd_order = 1;
            
            av_log(s, AV_LOG_VERBOSE, "video_rep_idx[%d]\n", (int)c->cur_video->rep_idx);
            av_log(s, AV_LOG_VERBOSE, "video_rep_count[%d]\n", (int)videoRepIdx);
        }
        
        if (c->cur_audio) {
            c->cur_audio->rep_count = audioRepIdx;

            av_log(s, AV_LOG_VERBOSE, "audio_rep_idx[%d]\n", (int)c->cur_audio->rep_idx);
            av_log(s, AV_LOG_VERBOSE, "audio_rep_count[%d]\n", (int)audioRepIdx);
        }
        c->nb_video_representations = videoRepIdx;
        c->nb_audio_representations = audioRepIdx;

cleanup:
        /*free the document */
        xmlFreeDoc(doc);
        // xmlCleanupParser();

    } else {
        av_log(s, AV_LOG_ERROR, "Unable to read to offset '%s'\n", url);
        ret = AVERROR_INVALIDDATA;
    }
    
    av_free(new_url);
    av_free(buffer);
    if (close_in) {
        avio_close(in);
    }
    return ret;
}

static int64_t get_segment_start_time_based_on_timeline( struct representation *pls, int64_t cur_seq_no ) {

    int64_t startTime = 0;
    if (pls->n_timelines) {
        
        int64_t num = 0;

        for (int64_t i = 0; i<pls->n_timelines; ++i) {
            if (pls->timelines[i]->t > 0) {
                startTime = pls->timelines[i]->t;
            }
            
            if (num == cur_seq_no)
                goto finish;
            
            startTime += pls->timelines[i]->d;
            
            for (int64_t j = 0; j < pls->timelines[i]->r; ++j) {
                num += 1;
                if (num == cur_seq_no)
                    goto finish;
                startTime += pls->timelines[i]->d;
            }
            num += 1;
            
        }
    }
finish:
    return startTime;
}


static int64_t calc_next_seg_no_from_timelines(struct representation *pls, int64_t currentTime)
{
    int64_t num = 0;
    int64_t startTime = 0;
    
    for ( int64_t i = 0; i<pls->n_timelines; ++i ) {
        if (pls->timelines[i]->t > 0) {
            startTime = pls->timelines[i]->t;

        }
        
        if (startTime > currentTime)
            goto finish;
        
        startTime += pls->timelines[i]->d;
        
        for ( int64_t j = 0; j < pls->timelines[i]->r; ++j ) {
            num += 1;
            if (startTime > currentTime)
                goto finish;
            startTime += pls->timelines[i]->d;
        }
        num += 1;
    }
    
    return -1;
    
finish:
    return num;
}

static int64_t calc_max_seg_no(struct representation *pls, DASHContext *c);

static void move_timelines(struct representation *rep_src, struct representation *rep_dest, DASHContext *c)
{
    if (rep_dest != NULL && rep_src != NULL) {
        free_timelines_list(rep_dest);
        rep_dest->timelines    = rep_src->timelines;
        rep_dest->n_timelines  = rep_src->n_timelines;
        rep_dest->first_seq_no = rep_src->first_seq_no;
        
        rep_dest->last_seq_no = calc_max_seg_no(rep_dest, c);
        
        rep_src->timelines = NULL;
        rep_src->n_timelines = 0;
        rep_dest->cur_seq_no = rep_src->cur_seq_no;
    }
}

static void move_segments(struct representation *rep_src, struct representation *rep_dest, DASHContext *c)
{
    if (rep_dest != NULL && rep_src != NULL) {

        free_segment_list(rep_dest);
        
        if (rep_src->start_number > (rep_dest->start_number + rep_dest->n_segments))
            rep_dest->cur_seq_no = 0;
        else
            rep_dest->cur_seq_no += rep_src->start_number - rep_dest->start_number;
        
        rep_dest->segments    = rep_src->segments;
        rep_dest->n_segments  = rep_src->n_segments;
        
        rep_dest->last_seq_no = calc_max_seg_no(rep_dest, c);
        
        rep_src->segments = NULL;
        rep_src->n_segments = 0;
    }
}

static int refresh_manifest(AVFormatContext *s)
{
    int ret = 0;
    DASHContext *c = s->priv_data;
    
    // save current context 
    struct representation *cur_video =  c->cur_video;
    struct representation *cur_audio =  c->cur_audio;
    char *base_url = c->base_url;
    
    //c->base_url = NULL;
    //c->cur_video = NULL;
    //c->cur_audio = NULL;
    
    av_log( c, AV_LOG_INFO, "---- Require Refreshing/Reloading, so now try to parse file with name[%s] ----\n", s->filename );

    #ifdef PRINTING
    printf( "%s---- Require Refreshing/Reloading, so now try to parse file with name[%s] ----\n", green_str, s->filename );
    #endif //PRINTING

    ret = parse_mainifest(s, s->filename, NULL);
    if (ret != 0) {
        
        av_log( c, AV_LOG_WARNING, "Failed to Refresh/Reload: Parse Manifest gave non-zero ret[%d] \n", ret );

        #ifdef PRINTING
        printf( "%sFailed to Refresh/Reload: Parse Manifest gave non-zero ret[%d] \n", red_str, ret );
        #endif //PRINTING
        
        goto finish;

    }
    
    if (cur_video && cur_video->timelines || cur_audio && cur_audio->timelines)
    {
        // calc current time 
        int64_t currentVideoTime = 0;
        int64_t currentAudioTime = 0;
        
        if (cur_video && cur_video->timelines)
            currentVideoTime = get_segment_start_time_based_on_timeline(cur_video, cur_video->cur_seq_no) / cur_video->segmentTimescalce;
        
        if (cur_audio && cur_audio->timelines)
            currentAudioTime = get_segment_start_time_based_on_timeline(cur_audio, cur_audio->cur_seq_no) / cur_audio->segmentTimescalce;
        
        // update segments
        if (cur_video && cur_video->timelines) {
            c->cur_video->cur_seq_no = calc_next_seg_no_from_timelines(c->cur_video, currentVideoTime * cur_video->segmentTimescalce - 1);
            if (c->cur_video->cur_seq_no >= 0)
                move_timelines(c->cur_video, cur_video, c);
        }
        
        if (cur_audio && cur_audio->timelines) {
            c->cur_audio->cur_seq_no = calc_next_seg_no_from_timelines(c->cur_audio, currentAudioTime * cur_audio->segmentTimescalce - 1);
            if (c->cur_audio->cur_seq_no >= 0)
                move_timelines(c->cur_audio, cur_audio, c);
        }
    }
    
    if (cur_video && cur_video->segments) {
        move_segments(c->cur_video, cur_video, c);
    }
    
    if (cur_audio && cur_audio->segments) {
        move_segments(c->cur_audio, cur_audio, c);
    }
    
finish:
    // restore context
    if (c->base_url)
        av_free(base_url);
    else
        c->base_url  = base_url;
    
    if (c->cur_audio)
        free_representation(c->cur_audio);
    
    if (c->cur_video)
        free_representation(c->cur_video);
    
    c->cur_audio = cur_audio;
    c->cur_video = cur_video;
    
    return ret;
}


static int64_t calc_cur_seg_no(struct representation *pls, DASHContext *c)
{
    int64_t num = 0;

    if (c->is_live) {
        if (pls->n_segments) {
            // handle segment number here
            num = pls->first_seq_no;
        } else if (pls->n_timelines) {
            int64_t startTimeOffset = get_segment_start_time_based_on_timeline(pls, 0xFFFFFFFF) - pls->timelines[pls->first_seq_no]->t; // total duration of playlist
            if (startTimeOffset < 60*pls->segmentTimescalce)
                startTimeOffset = 0;
            else
                startTimeOffset = startTimeOffset - 60*pls->segmentTimescalce;
            
            num = calc_next_seg_no_from_timelines(pls, pls->timelines[pls->first_seq_no]->t + startTimeOffset);
            if (num == -1)
                num = pls->first_seq_no;
        }
        else if (pls->segmentDuration) {
            num = pls->first_seq_no + (((GetCurrentTimeInSec() - c->availabilityStartTimeSec - c->presentationDelaySec) * pls->segmentTimescalce) - pls->presentationTimeOffset) / pls->segmentDuration;
            #ifdef PRINTING
            printf("pls->first_seq_no = %d\n", pls->first_seq_no);
            printf("pls->segmentTimescalce = %d\n", pls->segmentTimescalce);
            printf("pls->segmentDuration = %d\n", pls->segmentDuration);
            printf("GetCurrentTimeInSec() = %d\n", GetCurrentTimeInSec());
            printf("c->presentationDelaySec = %d\n", c->presentationDelaySec);
            printf("pls->presentationTimeOffset = %d\n", pls->presentationTimeOffset);
            printf("c->availabilityStartTimeSec = %d\n", c->availabilityStartTimeSec);
            printf("num = %d\n", num);
            #endif // PRINTING
        }
        else {
            num = pls->first_seq_no;
        }

    } else {
        num = pls->first_seq_no;
    }
    return num;
}

static int64_t calc_min_seg_no(struct representation *pls, DASHContext *c)
{
    int64_t num = 0;

    if (c->is_live && pls->segmentDuration) {
        num = pls->first_seq_no + (((GetCurrentTimeInSec() - c->availabilityStartTimeSec - c->timeShiftBufferDepthSec) * pls->segmentTimescalce) - pls->presentationTimeOffset) / pls->segmentDuration;
    } else {
        num = pls->first_seq_no;
    }
    return num;
}

static int64_t calc_max_seg_no(struct representation *pls, DASHContext *c)
{
    
    int64_t num = 0;

    if (pls->n_segments) {
        num = pls->first_seq_no + pls->n_segments - 1;
    } else if (pls->n_timelines) {
        num = pls->first_seq_no + pls->n_timelines - 1;
        for ( int i = 0; i < pls->n_timelines; ++i ) {
            num += pls->timelines[i]->r;
        }
    }
    else if (c->is_live && pls->segmentDuration) {
        num = pls->first_seq_no + (((GetCurrentTimeInSec() - c->availabilityStartTimeSec) * pls->segmentTimescalce) - pls->presentationTimeOffset) / pls->segmentDuration;
    }
    else if (pls->segmentDuration) {
        num = pls->first_seq_no + (c->mediaPresentationDurationSec * pls->segmentTimescalce) / pls->segmentDuration;
    }
    else {
        num = pls->first_seq_no;
    }
    return num;
}

static struct segment *get_current_segment(struct representation *pls)
{
    struct segment *seg = NULL;
    DASHContext *c = pls->parent->priv_data;
    
    while (!ff_check_interrupt(c->interrupt_callback) && pls->n_segments > 0) {
        if (pls->cur_seq_no < pls->n_segments) {
            struct segment *seg_ptr = pls->segments[pls->cur_seq_no];
            seg = av_mallocz(sizeof(struct segment));
            seg->url = av_strdup(seg_ptr->url);
            seg->size = seg_ptr->size;
            seg->url_offset = seg_ptr->url_offset;

            return seg;
        } else if (c->is_live) {
            #ifdef PRINTING
            printf( "%sSleeping Hitpoint 1 \n", cyan_str );
            #endif //PRINTING
            sleep(2);
            refresh_manifest(pls->parent);
        } else
            break;
    }
    
    if (c->is_live) {
        
        #ifdef PRINTING
        printf("%sget_current_segment (is_live)\n" , blue_str );
        #endif //PRINTING

        av_log( c, AV_LOG_VERBOSE, "get_current_segment (is_live)\n" );

        while ( !( ff_check_interrupt( c->interrupt_callback ) ) )  { // Updated Patch's Loop @ShahzadLone
        /* USED IN OLD PATCH

        while (1) {
            min_seq_no = calc_min_seg_no(pls->parent, pls);
            max_seq_no = calc_max_seg_no(pls->parent, pls);
        */
            int64_t min_seq_no = calc_min_seg_no(pls, c);
            int64_t max_seq_no = calc_max_seg_no(pls, c);

            av_log( c, AV_LOG_DEBUG, "[HIT 1](enter while) with min[%d], cur[%d], max[%d]\n", min_seq_no, pls->cur_seq_no, max_seq_no );

            #ifdef PRINTING
            printf( "%s[HIT 1](enter while) with min[%d], cur[%d], max[%d]\n", yellow_str, min_seq_no, pls->cur_seq_no, max_seq_no);
            #endif //PRINTING

            if (pls->cur_seq_no < min_seq_no) {

                av_log( c, AV_LOG_DEBUG, "[HIT 2](if [cur < min] case)\n" );
               
                #ifdef PRINTING
                printf("%s[HIT 2](if [cur < min] case)\n", cyan_str);
                #endif //PRINTING

                av_log(pls->parent, AV_LOG_VERBOSE, "%s to old segment: cur[%"PRId64"] min[%"PRId64"] max[%"PRId64"], playlist %d\n", __FUNCTION__, (int64_t)pls->cur_seq_no, min_seq_no, max_seq_no, (int)pls->rep_idx);
               
                if ( c->is_live && ( ( pls->timelines ) ||
                                     ( pls->segments )  ||
                                     ( pls->tmp_url_type == TMP_URL_TYPE_NUMBER ) 
                                   ) 
                   ) {
                    refresh_manifest(pls->parent);
                }
                // User picks which segment to fetch
                if (c->live_start_index == 0 || !c->is_live)
                    pls->cur_seq_no = calc_cur_seg_no(pls, c);
                else if (c->live_start_index < 0)
                    pls->cur_seq_no = pls->last_seq_no + c->live_start_index + 1;
                else if (c->live_start_index > 0)
                    pls->cur_seq_no = pls->first_seq_no + c->live_start_index - 1;
            }

            else if (pls->cur_seq_no == min_seq_no) { // Don't Refresh this case. @Shahzad for info!

                av_log( c, AV_LOG_DEBUG, "[HIT MIN EQUAL CASE](if [cur == min] case)\n" );
               
                #ifdef PRINTING
                printf("%s[HIT MIN EQUAL CASE](if [cur == min] case)\n", cyan_str);
                #endif //PRINTING

                av_log(pls->parent, AV_LOG_VERBOSE, "%s to old segment: cur[%"PRId64"] min[%"PRId64"] max[%"PRId64"], playlist %d\n", __FUNCTION__, (int64_t)pls->cur_seq_no, min_seq_no, max_seq_no, (int)pls->rep_idx);

                // User picks which segment to fetch
                if (c->live_start_index == 0 || !c->is_live)
                    pls->cur_seq_no = calc_cur_seg_no(pls, c);
                else if (c->live_start_index < 0)
                    pls->cur_seq_no = pls->last_seq_no + c->live_start_index + 1;
                else if (c->live_start_index > 0)
                    pls->cur_seq_no = pls->first_seq_no + c->live_start_index - 1;
            }  

            else if (pls->cur_seq_no > max_seq_no) {

                av_log( pls->parent, AV_LOG_DEBUG, "[HIT 3](Else if [cur > max] case)\n" );

                #ifdef PRINTING
                printf("%s[HIT 3](Else if [cur > max] case)\n", cyan_str);
                #endif //PRINTING

                av_log(c, AV_LOG_VERBOSE, "%s wait for new segment: min[%"PRId64"] max[%"PRId64"], playlist %d\n", __FUNCTION__, min_seq_no, max_seq_no, (int)pls->rep_idx);
                
                #ifdef PRINTING
                printf( "%sSleeping Hitpoint 2 \n", cyan_str );
                #endif //PRINTING
                sleep(.2); // Changed from sleep(2) to Make the Stream Smoother and not Lag. @ShahzadLone
                if ( c->is_live && ( ( pls->timelines ) ||
                                     ( pls->segments )  ||
                                     ( pls->tmp_url_type == TMP_URL_TYPE_NUMBER ) 
                                   ) 
                   ) {
                    refresh_manifest(pls->parent);
                }

                continue;
            }

            av_log( c, AV_LOG_DEBUG, "[HIT 4](break)\n" );

            #ifdef PRINTING
            printf( "%s[HIT 4](break)\n", yellow_str);
            #endif //PRINTING

            break;
        } // End of While-Loop

        seg = av_mallocz(sizeof(struct segment));

    } // End of if (c->is_live) case. 

    else if (pls->cur_seq_no <= pls->last_seq_no) {
        seg = av_mallocz(sizeof(struct segment));
    }
    
    if (seg) {
        if (pls->tmp_url_type != TMP_URL_TYPE_UNSPECIFIED) {

            int64_t val = pls->tmp_url_type == TMP_URL_TYPE_NUMBER ? pls->cur_seq_no : get_segment_start_time_based_on_timeline(pls, pls->cur_seq_no);

            int size = snprintf(NULL, 0, pls->url_template_format, val); // calc needed buffer size
            if (size > 0) {
                char *tmp_val = av_mallocz(size + 1);
                snprintf(tmp_val, size + 1, pls->url_template_format, val);
                seg->url = repl_str(pls->url_template, pls->url_template_pattern, tmp_val);
                av_free(tmp_val);
            }
            // av_log(pls->parent, AV_LOG_ERROR, "Invalid Segment Filename URL Template: %s\n", pls->url_template );

            // Check to make sure we got the right tmp_url_type and if not then handle the errors.
            if (pls->tmp_url_type == TMP_URL_TYPE_NUMBER) {
                av_log(pls->parent, AV_LOG_VERBOSE, "SUPPORTED : Templete URL is of [Number] type. \n");
            } else if (pls->tmp_url_type == TMP_URL_TYPE_TIME) {
                av_log(pls->parent, AV_LOG_VERBOSE, "SUPPORTED : Templete URL is of [Time] type. \n");
            } else { 
                av_log(pls->parent, AV_LOG_ERROR, "ERROR : Templete URL of this type [%u] is not supported! \n", pls->tmp_url_type);
                return( NULL ); // ?? not sure
            }

        } 

        if (!seg->url) {

            av_log(pls->parent, AV_LOG_ERROR, "Unable to resolve template url [%s] \n", pls->url_template);

            seg->url = av_strdup(pls->url_template);
        }
    }
    
    return seg;
}

enum ReadFromURLMode {
    READ_NORMAL,
    READ_COMPLETE,
};

static int read_from_url(struct representation *pls, struct segment *seg,
                         uint8_t *buf, int buf_size,
                         enum ReadFromURLMode mode)
{
    int ret;

     /* limit read if the segment was only a part of a file */
    if (seg->size >= 0)
        buf_size = FFMIN(buf_size, pls->cur_seg_size - pls->cur_seg_offset);

    if (mode == READ_COMPLETE) {
        ret = avio_read(pls->input, buf, buf_size);
        if (ret != buf_size)
            av_log(NULL, AV_LOG_ERROR, "Could not read complete segment buf_size[%d] ret[%d].\n", buf_size, ret);
    } else if (buf_size > 0)
        ret = avio_read(pls->input, buf, buf_size);
    else
        ret = AVERROR_EOF;

    if (ret > 0)
        pls->cur_seg_offset += ret;

    return ret;
}

static int open_input(DASHContext *c, struct representation *pls, struct segment *seg)
{

    #ifdef PRINTING
    printf(" ====== open_input ====== \n");
    printf("seg->url: %s\n", seg->url);
    printf(" ====== open_input ====== \n");
    #endif // PRINTING

    AVDictionary *opts = NULL;
    char url[MAX_URL_SIZE];
    int ret;

    // broker prior HTTP options that should be consistent across requests
    av_dict_set(&opts, "user-agent", c->user_agent, 0);
    av_dict_set(&opts, "cookies", c->cookies, 0);
    av_dict_set(&opts, "headers", c->headers, 0);
    if (c->is_live) {
        av_dict_set(&opts, "seekable", "0", 0);
    }

    if (seg->size >= 0) {
        /* try to restrict the HTTP request to the part we want
         * (if this is in fact a HTTP request) */
        av_dict_set_int(&opts, "offset", seg->url_offset, 0);
        av_dict_set_int(&opts, "end_offset", seg->url_offset + seg->size, 0);
    }

    ff_make_absolute_url(url, MAX_URL_SIZE, c->base_url, seg->url);

    // Calculating Segment Size (in Bytes). Using ffurl_seek is much faster than avio_size
    URLContext* urlCtx;
    //int ret = ffurl_alloc(&urlCtx, "", 0, 0);
    if (ffurl_open(&urlCtx, url, 0, 0, NULL) >= 0)
        seg->size = ffurl_seek(urlCtx, 0, AVSEEK_SIZE);
    else
        seg->size = -1;
    ffurl_close(urlCtx);
    av_log(NULL, AV_LOG_DEBUG, "Seg: url: %s,  size = %d\n", url, seg->size);

    av_log(pls->parent, AV_LOG_VERBOSE, "DASH request for url '%s', offset %"PRId64", playlist %d\n",
           url, seg->url_offset, pls->rep_idx);
    
    ret = open_url(pls->parent, &pls->input, url, c->avio_opts, opts, NULL);
    if (ret < 0) {
        goto cleanup;
    }
    
    /* Seek to the requested position. If this was a HTTP request, the offset
     * should already be where want it to, but this allows e.g. local testing
     * without a HTTP server. */
    if (ret == 0 && seg->url_offset) {
        int64_t seekret = avio_seek(pls->input, seg->url_offset, SEEK_SET);
        if (seekret < 0) {
            av_log(pls->parent, AV_LOG_ERROR, "Unable to seek to offset %"PRId64" of DASH segment '%s'\n", seg->url_offset, seg->url);
            ret = (int) seekret;
            ff_format_io_close(pls->parent, &pls->input);
        }
    }
    
cleanup:
    av_dict_free(&opts);
    pls->cur_seg_offset = 0;
    pls->cur_seg_size = seg->size;
    return ret;
}

static int update_init_section(struct representation *pls)
{
    static const int max_init_section_size = 1024*1024;
    DASHContext *c = pls->parent->priv_data;
    int64_t sec_size;
    int64_t urlsize;
    int ret;

    /* read init section only once per representation */
    if (!pls->init_section || pls->init_sec_buf) {
        return 0;
    }

    ret = open_input(c, pls, pls->init_section);
    if (ret < 0) {
        av_log(pls->parent, AV_LOG_WARNING,
               "Failed to open an initialization section in playlist %d\n",
               pls->rep_idx);
        return ret;
    }

    if (pls->init_section->size >= 0)
        sec_size = pls->init_section->size;
    else if ((urlsize = avio_size(pls->input)) >= 0)
        sec_size = urlsize;
    else
        sec_size = max_init_section_size;

    av_log(pls->parent, AV_LOG_DEBUG,
           "Downloading an initialization section of size %"PRId64"\n",
           sec_size);

    sec_size = FFMIN(sec_size, max_init_section_size);

    av_fast_malloc(&pls->init_sec_buf, &pls->init_sec_buf_size, sec_size);

    ret = read_from_url(pls, pls->init_section, pls->init_sec_buf,
                        pls->init_sec_buf_size, READ_COMPLETE);
    ff_format_io_close(pls->parent, &pls->input);

    if (ret < 0)
        return ret;
    if (pls->fix_multiple_stsd_order && pls->rep_idx > 0) {
        uint8_t **stsd_entries = NULL;
        int *stsd_entries_size = NULL;
        
        int i = 4;
        while(i <= ret-4) {
            // find start stsd atom
            if (0 == memcmp(pls->init_sec_buf + i, "stsd", 4)) {
                // 1B version
                // 3B flags
                // 4B num of entries
                int stsd_first_offset = i + 8;
                int stsd_offset = 0;
                int j = 0;
                uint32_t stsd_count = AV_RB32(pls->init_sec_buf + stsd_first_offset);
                stsd_first_offset += 4;
                if (stsd_count != pls->rep_count) {
                    i += 1;
                    continue;
                }
                
                // find all stsd entries
                stsd_entries = av_mallocz_array(stsd_count, sizeof(*stsd_entries));
                stsd_entries_size = av_mallocz_array(stsd_count, sizeof(*stsd_entries_size));
                for ( j = 0; j < stsd_count; ++j) {
                    // 4B - size
                    // 4B - format
                    stsd_entries_size[j] = AV_RB32(pls->init_sec_buf + stsd_first_offset + stsd_offset);
                    stsd_entries[j] = av_malloc(stsd_entries_size[j]);
                    memcpy(stsd_entries[j], pls->init_sec_buf + stsd_first_offset + stsd_offset, stsd_entries_size[j]);
                    stsd_offset += stsd_entries_size[j];
                }
                
                // reorder stsd entries
                // as first put stsd entry for current representation
                j = pls->rep_idx;
                stsd_offset = stsd_first_offset;
                memcpy(pls->init_sec_buf + stsd_offset, stsd_entries[j], stsd_entries_size[j]);
                stsd_offset += stsd_entries_size[j];
                
                for ( j = 0; j < stsd_count; ++j) {
                    if (j != pls->rep_idx) {
                        memcpy(pls->init_sec_buf + stsd_offset, stsd_entries[j], stsd_entries_size[j]);
                        stsd_offset += stsd_entries_size[j];
                    }
                    av_free(stsd_entries[j]);
                }
                
                av_freep(&stsd_entries);
                av_freep(&stsd_entries_size);
                break;
            }
            i += 1;
        }
    }

    av_log(pls->parent, AV_LOG_TRACE, "%s pls[%p] init section size[%d]\n", __FUNCTION__, pls, (int)ret);
    pls->init_sec_data_len = ret;
    pls->init_sec_buf_read_offset = 0;
    
    return 0;
}

static int64_t seek_data(void *opaque, int64_t offset, int whence)
{
    struct representation *v = opaque;
    if (v->n_segments == 1 && 0 == v->init_sec_data_len) {
        return avio_seek(v->input, offset, whence);
    }
    
    return AVERROR(ENOSYS);
}

struct AVIOInternal {
    URLContext *h;
};

static int read_data(void *opaque, uint8_t *buf, int buf_size)
{
    int ret = 0;
    struct representation *v = opaque;
    DASHContext *c = v->parent->priv_data;
    struct AVIOInternal* interal = NULL;
    URLContext* urlc = NULL;

    #ifdef PRINTING
    printf(" ====== read_data ====== \n");
    printf("Representation Type = %s\n", v->url_template);
    printf(" ====== read_data ====== \n");
    #endif // PRINTING
    // keep reference of mpegts parser callback mechanism

    if(v->input) {

        interal = (struct AVIOInternal*)v->input->opaque;
        urlc = (URLContext*)interal->h;
        v->mpegts_parser_input_backup = urlc->mpegts_parser_injection;
        v->mpegts_parser_input_context_backup = urlc->mpegts_parser_injection_context;
    }

restart:
    if (!v->input) {

        free_segment(&v->cur_seg);
        
        v->cur_seg = get_current_segment(v);

        if (!v->cur_seg) {
            ret = AVERROR_EOF;
            goto end;
        }

        /* load/update Media Initialization Section, if any */
        ret = update_init_section(v);
        if (ret)
            goto end;

        av_log( v->parent, AV_LOG_INFO, "Try to Open Input: %s \n", v->cur_seg->url);

        #ifdef PRINTING
        printf("%sTry to Open Input: %s \n", green_str, v->cur_seg->url);
        #endif //PRINTING

        ret = open_input(c, v, v->cur_seg);
        if (ret < 0) {
            av_log( v->parent, AV_LOG_WARNING, "Failed to Open Input by (open_input): %s \n", v->cur_seg->url);

            #ifdef PRINTING
            printf( "%sFailed to Open Input by (open_input): %s \n", red_str, v->cur_seg->url );
            #endif //PRINTING

            if (ff_check_interrupt(c->interrupt_callback)) {
                goto end;
                ret = AVERROR_EXIT;
            }

            av_log(v->parent, AV_LOG_WARNING, "Failed to Open Segment of the following Playlist [%d] \n", v->rep_idx);

            #ifdef PRINTING
            printf( "%sFailed to Open Segment of the following Playlist [%d] \n", red_str, v->rep_idx );
            #endif //PRINTING

            if ( c->is_live && ( ( v->timelines ) ||
                                 ( v->segments )  ||
                                 ( v->tmp_url_type == TMP_URL_TYPE_NUMBER ) 
                               ) 
               ) {
            
                #ifdef PRINTING
                printf( "%sSleeping Hitpoint 3 \n", cyan_str );
                #endif //PRINTING
                sleep(.2); // Changed from sleep(1), and put it before refreshing.

                refresh_manifest(v->parent); // Uncommented, not sure why it was commented(this used to be before the above sleep). 
            }
            else {
                v->cur_seq_no += 1;
            }

            goto restart;
        }
    }

    if (v->init_sec_buf_read_offset < v->init_sec_data_len) {
        /* Push init section out first before first actual segment */
        int copy_size = FFMIN(v->init_sec_data_len - v->init_sec_buf_read_offset, buf_size);
        memcpy(buf, v->init_sec_buf, copy_size);
        v->init_sec_buf_read_offset += copy_size;
        ret = copy_size;
        goto end;
    }
    
    if (!v->cur_seg) {
        v->cur_seg = get_current_segment(v);
    }
    
    if (!v->cur_seg) {
        ret = AVERROR_EOF;
        goto end;
    }
    
    ret = read_from_url(v, v->cur_seg, buf, buf_size, READ_NORMAL);
    if (ret > 0)
        goto end;
    
    /* This is needed to free indexes from previously processed segments.
     * Maybe we should restart context every few segments?
     */
    if (!v->is_restart_needed)
        v->cur_seq_no += 1;
    v->is_restart_needed = 1;
    
    // ??? Commented out in the upgraded patch from before not sure why.
    // ff_format_io_close(v->parent, &v->input); 
    // v->cur_seq_no += 1;
    // goto restart;

end:

    // replace mpegts parser callback mechanism
    if(interal && urlc && v->input) {
        interal = (struct AVIOInternal*)v->input->opaque;
        urlc = (URLContext*)interal->h;
        urlc->mpegts_parser_injection = v->mpegts_parser_input_backup;
        urlc->mpegts_parser_injection_context = v->mpegts_parser_input_context_backup;
    }

    return ret;
}

static int save_avio_options(AVFormatContext *s)
{
    DASHContext *c = s->priv_data;
    const char *opts[] = { "headers", "user_agent", "user-agent", "cookies", NULL }, **opt = opts;
    uint8_t *buf = NULL;
    int ret = 0;

    while (*opt) {
        if (av_opt_get(s->pb, *opt, AV_OPT_SEARCH_CHILDREN, &buf) >= 0) {
            if (buf != NULL && buf[0] != '\0') {
                ret = av_dict_set(&c->avio_opts, *opt, buf,
                                  AV_DICT_DONT_STRDUP_VAL);
                if (ret < 0)
                    return ret;
            }
        }
        opt++;
    }

    return ret;
}

static int nested_io_open(AVFormatContext *s, AVIOContext **pb, const char *url, int flags, AVDictionary **opts)
{
    av_log(s, AV_LOG_ERROR,
           "A DASH playlist item '%s' referred to an external file '%s'. "
           "Opening this file was forbidden for security reasons\n",
           s->filename, url);
    return AVERROR(EPERM);
}

static int reopen_demux_for_component(AVFormatContext *s, struct representation *pls)
{    
    DASHContext *c = s->priv_data;
    AVInputFormat *in_fmt = NULL;
    AVDictionary  *in_fmt_opts = NULL;
    uint8_t *avio_ctx_buffer  = NULL;
    int ret = 0;
    
    if (pls->ctx) {
        /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
        av_freep(&pls->pb.buffer);
        memset(&pls->pb, 0x00, sizeof(AVIOContext));
        
        pls->ctx->pb = NULL;
        avformat_close_input(&pls->ctx);
        pls->ctx = NULL;
    }
    
    if (!(pls->ctx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    avio_ctx_buffer  = av_malloc(INITIAL_BUFFER_SIZE);
    if (!avio_ctx_buffer ){
        ret = AVERROR(ENOMEM);
        avformat_free_context(pls->ctx);
        pls->ctx = NULL;
        goto fail;
    }
    
    if (c->is_live) {
        ffio_init_context(&pls->pb, avio_ctx_buffer , INITIAL_BUFFER_SIZE, 0, pls,
                          read_data, NULL, NULL);
    } else {
        ffio_init_context(&pls->pb, avio_ctx_buffer , INITIAL_BUFFER_SIZE, 0, pls,
                  read_data, NULL, seek_data);
    }
    
    pls->pb.seekable = 0;

    if ((ret = ff_copy_whiteblacklists(pls->ctx, s)) < 0)
        goto fail;

    pls->ctx->flags = AVFMT_FLAG_CUSTOM_IO;
    pls->ctx->probesize = 1024 * 4;
    pls->ctx->max_analyze_duration = 4 * AV_TIME_BASE; 
    /////////////////
#if 1
    ret = av_probe_input_buffer(&pls->pb, &in_fmt, "",
                                NULL, 0, 0);
    if (ret < 0) {
        av_log(s, AV_LOG_ERROR, "Error when loading first segment, playlist %d\n", (int)pls->rep_idx);
        avformat_free_context(pls->ctx);
        pls->ctx = NULL;
        goto fail;
    }
#else
    in_fmt = av_find_input_format("mp4");
#endif

    pls->ctx->pb = &pls->pb;
    pls->ctx->io_open  = nested_io_open;
    ////////////////////
    
    // provide additional information from mpd if available
    // av_dict_set(&in_fmt_opts, "video_size", "640x480", 0);
    // av_dict_set(&in_fmt_opts, "pixel_format", "rgb24", 0);
    ret = avformat_open_input(&pls->ctx, "", in_fmt, &in_fmt_opts); //pls->init_section->url
    av_dict_free(&in_fmt_opts);
    if (ret < 0)
        goto fail;
    
    if (pls->n_segments == 1) {
        //pls->ctx->ctx_flags &= ~AVFMTCTX_NOHEADER;
        ret = avformat_find_stream_info(pls->ctx, NULL);
        if (ret < 0)
            goto fail;
    }
        
    av_log(pls->parent, AV_LOG_VERBOSE, "%s nb_streams[%d]\n", __FUNCTION__, (int)pls->ctx->nb_streams);
    
fail:
    return ret;
}

static int open_demux_for_component(AVFormatContext *s, struct representation *pls, int rep_idx)
{
    int ret = 0;

    DASHContext *c = s->priv_data;
    
    pls->parent = s;
    pls->cur_seq_no = calc_cur_seg_no(pls, c);
    pls->last_seq_no = calc_max_seg_no(pls, c);
    // User picks which segment to fetch
    if (c->live_start_index == 0 || !c->is_live)
        pls->cur_seq_no = calc_cur_seg_no(pls, c);
    else if (c->live_start_index < 0)
        pls->cur_seq_no = pls->last_seq_no + c->live_start_index + 1;
    else if (c->live_start_index > 0)
        pls->cur_seq_no = pls->first_seq_no + c->live_start_index - 1;

    ret = reopen_demux_for_component(s, pls);
    if (ret < 0) {
        goto fail;
    }
    
    for (int i = 0; i < pls->ctx->nb_streams; ++i) {
        AVStream *st = avformat_new_stream(s, NULL);
        AVStream *ist = pls->ctx->streams[i];
        if (!st) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }
        st->id = i;

        // avcodec_copy_context(st->codec, pls->ctx->streams[i]->codec);
        
        // Added default Pixel Format of YUV420P in case not initialized. 
        if (pls->ctx->streams[i]->codecpar->format == AV_PIX_FMT_NONE)
            pls->ctx->streams[i]->codecpar->format = AV_PIX_FMT_YUV420P; //DEFAULT PIX FORMAT

        if (pls->ctx->streams[i]->codec->pix_fmt == AV_PIX_FMT_NONE)
            pls->ctx->streams[i]->codec->pix_fmt = AV_PIX_FMT_YUV420P; //DEFAULT PIX FORMAT

        avcodec_parameters_copy(st->codecpar, pls->ctx->streams[i]->codecpar);
        avcodec_copy_context(st->codec, pls->ctx->streams[i]->codec);

        // Make stream Indices same as Rep Indices (for uniqueness). TODO: what if multiple streams per representation
        st->id = rep_idx;

        av_log(NULL, AV_LOG_VERBOSE, "st->codec->pix_fmt = %d\n", st->codec->pix_fmt);
        av_log(NULL, AV_LOG_VERBOSE, "st->codecpar->format = %d\n", st->codecpar->format);
        
        avpriv_set_pts_info(st, ist->pts_wrap_bits, ist->time_base.num, ist->time_base.den);
    }   

    return 0;

fail:
    return ret;
}

static int dash_read_header(AVFormatContext *s)
{
    void *u = (s->flags & AVFMT_FLAG_CUSTOM_IO) ? NULL : s->pb;
    DASHContext *c = s->priv_data;
    int ret = 0;
    int stream_index = 0;

    c->interrupt_callback = &s->interrupt_callback;

    // if the URL context is good, read important options we must broker later
    if (u) {
        update_options(&c->user_agent, "user-agent", u);
        update_options(&c->cookies, "cookies", u);
        update_options(&c->headers, "headers", u);
    }

    if ( ( ret = parse_mainifest( s, s->filename, s->pb ) ) < 0 ) {
        goto fail;
    }

    if ( ( ret = save_avio_options( s ) ) < 0 ) {
        goto fail;
    }

    /* If this isn't a live stream, fill the total duration of the
     * stream. */
    if (!c->is_live) {

        s->duration = (int64_t) c->mediaPresentationDurationSec * AV_TIME_BASE;
    }

    /* Open the demuxer for curent video and current audio components if available "UPGRADED PATCH WAY" */
    if ( ( 0 == ret ) && ( c->cur_video ) ) {
        #ifdef PRINTING
        printf("====== open_demux_for_component ===== \n");
        printf(" Video ");
        printf("====== open_demux_for_component ===== \n");
        #endif // PRINTING
        ret = open_demux_for_component(s, c->cur_video, c->cur_video->rep_idx);
        if (ret == 0) {
            c->cur_video->stream_index = stream_index;
            ++stream_index;
        } else {
            free_representation(c->cur_video);
            c->cur_video = NULL;
        }
    }
    
    if ( ( 0 == ret ) && ( c->cur_audio ) ) {
        #ifdef PRINTING
        printf("====== open_demux_for_component ===== \n");
        printf(" Audio ");
        printf("====== open_demux_for_component ===== \n");
        #endif // PRINTING
        ret = open_demux_for_component(s, c->cur_audio, c->cur_audio->rep_idx);
        if (ret == 0) {
            c->cur_audio->stream_index = stream_index;
            ++stream_index;
        } else {
            free_representation(c->cur_audio);
            c->cur_audio = NULL;
        }
    }

    if (0 == stream_index) {

        ret = AVERROR_INVALIDDATA;
        goto fail;
    }
    
    /* Create a program */
    if (0 == ret) {
        
        AVProgram *program;
        program = av_new_program(s, 0);
        if (!program) {
            goto fail;
        }

        if (c->cur_video) {
            av_program_add_stream_index(s, 0, c->cur_video->stream_index);
        }

        if (c->cur_audio) {
            av_program_add_stream_index(s, 0, c->cur_audio->stream_index);
        }

    }

    return 0;
fail:
    return ret;
}


static int dash_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    DASHContext *c = s->priv_data;
    int ret = 0;
    struct representation *cur = NULL;
    
    if (!c->cur_audio && !c->cur_video) {
        return AVERROR_INVALIDDATA;
    }
    
    if (c->cur_audio && !c->cur_video) {
        cur = c->cur_audio;
    } else if (!c->cur_audio && c->cur_video) {
        cur = c->cur_video;
    } else if (c->cur_video->cur_timestamp > c->cur_audio->cur_timestamp) {
        cur = c->cur_audio;
    } else {
        cur = c->cur_video;
    }

    if (cur->ctx) {
        while (!ff_check_interrupt(c->interrupt_callback) && ret == 0) {
            ret = av_read_frame(cur->ctx, &cur->pkt);
            
            if (0 == ret) {
                // If we got a packet, return it.
                *pkt = cur->pkt;
                cur->cur_timestamp = av_rescale(pkt->pts, (int64_t)cur->ctx->streams[0]->time_base.num * 90000, cur->ctx->streams[0]->time_base.den);
                pkt->stream_index = cur->stream_index;
                reset_packet(&cur->pkt);
                return 0;
            } 
            
            reset_packet(&cur->pkt);
            
            if (cur->is_restart_needed) {
                while (!ff_check_interrupt(c->interrupt_callback)) {
                    cur->cur_seg_offset = 0;
                    cur->init_sec_buf_read_offset = 0;
                    if (cur->input)
                        ff_format_io_close(cur->parent, &cur->input);
                    ret = reopen_demux_for_component(s, cur);
                    if (c->is_live && ret != 0) {
                        #ifdef PRINTING
                        printf( "%sSleeping Hitpoint 4 \n", cyan_str );
                        #endif //PRINTING
                        sleep(2);
                        continue;
                    }
                    break;
                }
                cur->is_restart_needed = 0;
            }
        }
    }
    
    return AVERROR_EOF;
}


static int dash_close(AVFormatContext *s)
{
    DASHContext *c = s->priv_data;
    
    if (c->cur_audio) {
        free_representation(c->cur_audio);
    }
    
    if (c->cur_video) {
        free_representation(c->cur_video);
    }
    
    av_freep(&c->cookies);
    av_freep(&c->user_agent);
    av_dict_free(&c->avio_opts);
    av_freep(&c->base_url);
    xmlCleanupParser();
    return 0;
}
    
static int dash_seek(AVFormatContext *s, struct representation *pls, int64_t seekPosMSec, int flags)
{
    int ret = 0;
    
    av_log(pls->parent, AV_LOG_VERBOSE, "DASH seek pos[%"PRId64"ms], playlist %d\n", seekPosMSec, pls->rep_idx);
    // single segment mode
    if (pls->n_segments == 1) {
        pls->cur_timestamp = 0;
        pls->cur_seg_offset = 0;
        ff_read_frame_flush(pls->ctx);
        return av_seek_frame(pls->ctx, -1, seekPosMSec*1000, flags);
    }
    
    if (pls->input)
        ff_format_io_close(pls->parent, &pls->input);
    
    // find the nearest segment
    if (pls->n_timelines > 0 && pls->segmentTimescalce > 0) {

        av_log(pls->parent, AV_LOG_VERBOSE, "dash_seek with SegmentTimeline start n_timelines[%d] last_seq_no[%"PRId64"], playlist %d.\n", (int)pls->n_timelines, (int64_t)pls->last_seq_no, (int)pls->rep_idx);
        
        int64_t duration = 0;
        int64_t num = pls->first_seq_no;

        for ( int i = 0; i < pls->n_timelines; ++i ) {
            if (pls->timelines[i]->t > 0) {
                duration = pls->timelines[i]->t;
            }
            
            duration += pls->timelines[i]->d;
            if (seekPosMSec < ((duration * 1000) /  pls->segmentTimescalce)) {
                goto set_seq_num;
            }
            
            for ( int j = 0; j < pls->timelines[i]->r; ++j ) {
                duration += pls->timelines[i]->d;
                num += 1;
                if (seekPosMSec < ((duration * 1000) /  pls->segmentTimescalce)) {
                    goto set_seq_num;
                }
            }
            num += 1;
        }
set_seq_num:
        pls->cur_seq_no = num > pls->last_seq_no ? pls->last_seq_no : num;
        av_log(pls->parent, AV_LOG_VERBOSE, "dash_seek with SegmentTimeline end cur_seq_no[%"PRId64"], playlist %d.\n", (int64_t)pls->cur_seq_no, (int)pls->rep_idx);
    }
    else if(pls->segmentDuration > 0) {
        pls->cur_seq_no = pls->first_seq_no + ((seekPosMSec * pls->segmentTimescalce) / pls->segmentDuration) / 1000;
    } else {
        av_log(pls->parent, AV_LOG_ERROR, "dash_seek missing segmentDuration\n");
        pls->cur_seq_no = pls->first_seq_no;
    }
    pls->cur_timestamp = 0;
    pls->cur_seg_offset = 0;
    
    pls->init_sec_buf_read_offset = 0;
    ret = reopen_demux_for_component(s, pls);

    return ret;
}

static int dash_read_seek( AVFormatContext *s, int stream_index, int64_t timestamp, int flags )
{
    int ret = 0;
    DASHContext *c = s->priv_data;
    int64_t seekPosMSec = av_rescale_rnd(timestamp, 1000,
                            s->streams[stream_index]->time_base.den,
                            flags & AVSEEK_FLAG_BACKWARD ?
                            AV_ROUND_DOWN : AV_ROUND_UP);
    
    if ((flags & AVSEEK_FLAG_BYTE) || c->is_live)
        return AVERROR(ENOSYS);
    
    if (c->cur_audio) {
        ret = dash_seek(s, c->cur_audio, seekPosMSec, flags);
    } 
    
    if (0 == ret && c->cur_video) {
        ret = dash_seek(s, c->cur_video, seekPosMSec, flags);
    }
    
    return ret;
}

static int dash_probe(AVProbeData *p)
{
    if (!strstr(p->buf, "<MPD"))
        return 0;
    
    if (strstr(p->buf, "dash:profile:isoff-on-demand:2011") ||
        strstr(p->buf, "dash:profile:isoff-live:2011") ||
        strstr(p->buf, "dash:profile:isoff-live:2012") ||
        strstr(p->buf, "dash:profile:isoff-main:2011") || 
        av_stristr(p->buf, "dash:schema:mpd") ) {
        return AVPROBE_SCORE_MAX;
    }
    
    if (strstr(p->buf, "dash:profile")) {
        return AVPROBE_SCORE_MAX / 2;
    }
    
    return 0;
}

#define OFFSET(x) offsetof(DASHContext, x)
#define FLAGS AV_OPT_FLAG_DECODING_PARAM

static const AVOption dash_options[] = {

    // Updated Patch Method Options.
    { "audio_rep_index", "audio representation index to be used", OFFSET(audio_rep_index), AV_OPT_TYPE_INT, {.i64 = -1}, INT_MIN, INT_MAX, FLAGS },
    { "video_rep_index", "video representation index to be used", OFFSET(video_rep_index), AV_OPT_TYPE_INT, {.i64 = -1}, INT_MIN, INT_MAX, FLAGS },
    { "video_rep_id", "selected representations"  , OFFSET(video_rep_id), AV_OPT_TYPE_STRING, {.str = ""}, INT_MIN, INT_MAX, FLAGS },
    { "audio_rep_id", "selected representations"  , OFFSET(audio_rep_id), AV_OPT_TYPE_STRING, {.str = ""}, INT_MIN, INT_MAX, FLAGS },
    { "live_start_index", "segment index to start live streams at (negative values are from the end)", OFFSET(live_start_index), AV_OPT_TYPE_INT, {.i64 = 0}, INT_MIN, INT_MAX, FLAGS},
  
    {NULL}

};

static const AVClass dash_class = {
    .class_name = "dash",
    .item_name  = av_default_item_name,
    .option     = dash_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVInputFormat ff_dash_demuxer = {
    .name           = "dash",
    .long_name      = NULL_IF_CONFIG_SMALL("Dynamic Adaptive Streaming over HTTP"),
    .priv_class     = &dash_class,
    .priv_data_size = sizeof(DASHContext),
    .read_probe     = dash_probe,
    .read_header    = dash_read_header,
    .read_packet    = dash_read_packet,
    .read_close     = dash_close,
    .read_seek      = dash_read_seek,
    .flags          = AVFMT_NO_BYTE_SEEK,
};
