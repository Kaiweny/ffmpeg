/*
 * Dynamic Adaptive Streaming over HTTP demux
 * Copyright (c) 2017 samsamsam@o2.pl based on HLS demux
 * Copyright (c) 2017 Steven Liu
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
#include <libxml/parser.h>
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"
#include "libavutil/time.h"
#include "libavutil/parseutils.h"
#include "internal.h"
#include "avio_internal.h"

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

//#define TESTING

char *av_strreplace(const char *str, const char *from, const char *to);

struct fragment {
    int64_t url_offset;
    int64_t size;
    char *url;
};

/*
 * reference to : ISO_IEC_23009-1-DASH-2012
 * Section: 5.3.9.6.2
 * Table: Table 17 — Semantics of SegmentTimeline element
 * */
struct timeline {
    /* t: Element or Attribute Name
     * specifies the MPD start time, in @timescale units,
     * the first Segment in the series starts relative to the beginning of the Period.
     * The value of this attribute must be equal to or greater than the sum of the previous S
     * element earliest presentation time and the sum of the contiguous Segment durations.
     * If the value of the attribute is greater than what is expressed by the previous S element,
     * it expresses discontinuities in the timeline.
     * If not present then the value shall be assumed to be zero for the first S element
     * and for the subsequent S elements, the value shall be assumed to be the sum of
     * the previous S element's earliest presentation time and contiguous duration
     * (i.e. previous S@t + @d * (@r + 1)).
     * */
    int64_t t;
    /* r: Element or Attribute Name
     * specifies the repeat count of the number of following contiguous Segments with
     * the same duration expressed by the value of @d. This value is zero-based
     * (e.g. a value of three means four Segments in the contiguous series).
     * */
    int64_t r;
    /* d: Element or Attribute Name
     * specifies the Segment duration, in units of the value of the @timescale.
     * */
    int64_t d;
};

enum DASHTmplUrlType {
    TMP_URL_TYPE_UNSPECIFIED,
    TMP_URL_TYPE_NUMBER,
    TMP_URL_TYPE_TIME,
};

/*
 * Each playlist has its own demuxer. If it is currently active,
 * it has an opened AVIOContext too, and potentially an AVPacket
 * containing the next packet from this stream.
 */
struct representation {
    char *url_template;
    enum DASHTmplUrlType tmp_url_type;
    AVIOContext pb;
    AVIOContext *input;
    AVFormatContext *parent;
    AVFormatContext *ctx;
    AVPacket pkt;
    int rep_idx;
    int rep_count;
    int stream_index;

    enum AVMediaType type;
    int64_t target_duration;

    int n_fragments;
    struct fragment **fragments; /* VOD list of fragment for profile */

    int n_timelines;
    struct timeline **timelines;

    int64_t first_seq_no;
    int64_t last_seq_no;

    int64_t fragment_duration;
    int64_t fragment_timescale;

    int64_t cur_seq_no;
    int64_t cur_seg_offset;
    int64_t cur_seg_size;
    struct fragment *cur_seg;

    /* Currently active Media Initialization Section */
    struct fragment *init_section;
    uint8_t *init_sec_buf;
    uint32_t init_sec_buf_size;
    uint32_t init_sec_data_len;
    uint32_t init_sec_buf_read_offset;
    int fix_multiple_stsd_order;
    int64_t cur_timestamp;

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
    int bandwidth;
};

typedef struct DASHContext {
    const AVClass *class;
    char *base_url;
    int nb_representations;
    struct representation **representations;
    struct representation *cur_video;
    struct representation *cur_audio;
    uint64_t media_presentation_duration_sec;
    uint64_t suggested_presentation_delay_sec;
    uint64_t presentation_delay_sec;
    uint64_t availability_start_time_sec;
    uint64_t publish_time_sec;
    uint64_t minimum_update_period_sec;
    uint64_t time_shift_buffer_depth_sec;
    uint64_t min_buffer_time_sec;
    uint64_t period_duration_sec;
    uint64_t period_start_sec;
    int is_live;
    AVIOInterruptCB *interrupt_callback;
    char *user_agent;                    ///< holds HTTP user agent set as an AVOption to the HTTP protocol context
    char *cookies;                       ///< holds HTTP cookie values set in either the initial response or as an AVOption to the HTTP protocol context
    char *headers;                       ///< holds HTTP headers set as an AVOption to the HTTP protocol context
    AVDictionary *avio_opts;
    int rep_index;
} DASHContext;


// prints the representation structure in green. @Shahzad for help!
static void print_rep_struct( struct representation *v ) {
    // For testing
    printf( "\n\n%sstruct representation *v {\n", green_str );

    printf( "%s    char *url_template = %s,\n", green_str, v->url_template );
    printf( "%s    AVIOContext *input = %d,\n", green_str, v->input );
    printf( "%s    enum DASHTmplUrlType tmp_url_type = %d,\n", green_str, v->tmp_url_type );
    printf( "%s    int rep_idx = %d,\n", green_str, v->rep_idx );
    printf( "%s    int rep_count = %d,\n", green_str, v->rep_count );
    printf( "%s    int stream_index = %d,\n", green_str, v->stream_index );
    printf( "%s    enum AVMediaType type = %d,\n", green_str, v->type );
    printf( "%s    int64_t target_duration = %d,\n", green_str, v->target_duration );
    printf( "%s    int n_fragments = %d,\n", green_str, v->n_fragments );
    printf( "%s    int n_timelines = %d,\n", green_str, v->n_timelines );
    //printf("%s    int64_t first_seq_no_in_representation = %d,\n", green_str, v->first_seq_no_in_representation );
    printf( "%s    int64_t first_seq_no = %d,\n", green_str, v->first_seq_no );
    printf( "%s    int64_t last_seq_no = %d,\n", green_str, v->last_seq_no );
    printf( "%s    int64_t fragment_duration = %d,\n", green_str, v->fragment_duration );
    printf( "%s    int64_t fragment_timescale = %d,\n", green_str, v->fragment_timescale );
    printf( "%s    int64_t cur_seq_no = %d,\n", green_str, v->cur_seq_no );
    printf( "%s    int64_t cur_seg_offset = %d,\n", green_str, v->cur_seg_offset );
    printf( "%s    int64_t cur_seg_size = %d,\n", green_str, v->cur_seg_size );
    printf( "%s    uint32_t init_sec_buf_size = %d,\n", green_str, v->init_sec_buf_size );
    printf( "%s    uint32_t init_sec_data_len = %d,\n", green_str, v->init_sec_data_len );
    printf( "%s    uint32_t init_sec_buf_read_offset = %d,\n", green_str, v->init_sec_buf_read_offset );
    printf( "%s    int fix_multiple_stsd_order = %d,\n", green_str, v->fix_multiple_stsd_order );
    printf( "%s    int64_t cur_timestamp = %d\n", green_str, v->cur_timestamp );

    //if ( (v->cur_seg_size) != -1 ) 
    { // Print the cur_seg within
        printf( "\n\n%s    struct fragment *cur_seg; {\n", green_str );

        printf( "%s        int64_t url_offset = %d,\n", green_str, v->url_template );
        printf( "%s        int64_t size = %d,\n", green_str, v->tmp_url_type );
        printf( "%s        char *url = %s,\n", green_str, v->rep_idx );

        printf( "%s    }\n", green_str );
    }

    printf( "%s}\n\n", green_str );
    // End for testing prints
}


char *av_strreplace(const char *str, const char *from, const char *to)
{
    char *ret = NULL;
    const char *pstr2, *pstr = str;
    size_t tolen = strlen(to), fromlen = strlen(from);
    AVBPrint pbuf;
    av_bprint_init(&pbuf, 1, AV_BPRINT_SIZE_UNLIMITED);
    while ((pstr2 = av_stristr(pstr, from))) {
        av_bprint_append_data(&pbuf, pstr, pstr2 - pstr);
        pstr = pstr2 + fromlen;
        av_bprint_append_data(&pbuf, to, tolen);
    }
    av_bprint_append_data(&pbuf, pstr, strlen(pstr));
    if (!av_bprint_is_complete(&pbuf)) {
        av_bprint_finalize(&pbuf, NULL);
    } else {
        av_bprint_finalize(&pbuf, &ret);
    }
    return ret;
}
static uint64_t get_current_time_in_sec(void)
{
    return  av_gettime() / 1000000;
}

static uint64_t get_utc_date_time_insec(AVFormatContext *s, const char *datetime)
{
    struct tm timeinfo;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int ret = 0;
    float second = 0.0;

    /* ISO-8601 date parser */
    if (!datetime)
        return 0;

    ret = sscanf(datetime, "%d-%d-%dT%d:%d:%fZ", &year, &month, &day, &hour, &minute, &second);
    /* year, month, day, hour, minute, second  6 arguments */
    if (ret != 6) {
        av_log(s, AV_LOG_WARNING, "get_utc_date_time_insec get a wrong time format\n");
    }
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon  = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min  = minute;
    timeinfo.tm_sec  = (int)second;

    return av_timegm(&timeinfo);
}

static uint32_t get_duration_insec(AVFormatContext *s, const char *duration)
{
    /* ISO-8601 duration parser */
    uint32_t days = 0;
    uint32_t hours = 0;
    uint32_t mins = 0;
    uint32_t secs = 0;
    uint32_t size = 0;
    float value = 0;
    uint8_t type = 0;
    const char *ptr = duration;

    while (*ptr) {
        if (*ptr == 'P' || *ptr == 'T') {
            ptr++;
            continue;
        }

        if (sscanf(ptr, "%f%c%n", &value, &type, &size) != 2) {
            av_log(s, AV_LOG_WARNING, "get_duration_insec get a wrong time format\n");
            return 0; /* parser error */
        }
        switch (type) {
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
        ptr += size;
    }
    return  ((days * 24 + hours) * 60 + mins) * 60 + secs;
}

static void free_fragment(struct fragment **seg)
{
    if (!(*seg)) {
        return;
    }
    av_freep(&(*seg)->url);
    av_freep(seg);
}

static void free_fragment_list(struct representation *pls)
{
    int i;

    for (i = 0; i < pls->n_fragments; i++) {
        free_fragment(&pls->fragments[i]);
    }
    av_freep(&pls->fragments);
    pls->n_fragments = 0;
}

static void free_timelines_list(struct representation *pls)
{
    int i;

    for (i = 0; i < pls->n_timelines; i++) {
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
    free_fragment_list(pls);
    free_timelines_list(pls);
    free_fragment(&pls->cur_seg);
    free_fragment(&pls->init_section);
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

    av_free(pls->url_template);
    av_free(pls);
}

static void update_options(char **dest, const char *name, void *src)
{
    av_freep(dest);
    av_opt_get(src, name, AV_OPT_SEARCH_CHILDREN, (uint8_t**)dest);
    if (*dest)
        av_freep(dest);
}

static int open_url(AVFormatContext *s, AVIOContext **pb, const char *url,
                    AVDictionary *opts, AVDictionary *opts2, int *is_http)
{
    DASHContext *c = s->priv_data;
    AVDictionary *tmp = NULL;
    const char *proto_name = NULL;
    int ret;
    void *p = NULL;

    av_dict_copy(&tmp, opts, 0);
    av_dict_copy(&tmp, opts2, 0);

    if (av_strstart(url, "crypto", NULL)) {
        if (url[6] == '+' || url[6] == ':')
            proto_name = avio_find_protocol_name(url + 7);
    }

    if (!proto_name)
        proto_name = avio_find_protocol_name(url);

    if (!proto_name)
        return AVERROR_INVALIDDATA;

    // only http(s) & file are allowed
    if (!av_strstart(proto_name, "http", NULL) && !av_strstart(proto_name, "file", NULL)) {
        return AVERROR_INVALIDDATA;
    }
    if (!strncmp(proto_name, url, strlen(proto_name)) && url[strlen(proto_name)] == ':') {
        ;
    } else if (av_strstart(url, "crypto", NULL) && !strncmp(proto_name, url + 7, strlen(proto_name)) && url[7 + strlen(proto_name)] == ':') {
        ;
    } else if (strcmp(proto_name, "file") || !strncmp(url, "file,", 5)) {
        return AVERROR_INVALIDDATA;
    }
    ret = s->io_open(s, pb, url, AVIO_FLAG_READ, &tmp);
    if (ret >= 0) {
        // update cookies on http response with setcookies.
        p = (s->flags & AVFMT_FLAG_CUSTOM_IO) ? NULL : s->pb;
        update_options(&c->cookies, "cookies", p);
        av_dict_set(&opts, "cookies", c->cookies, 0);
    }

    av_dict_free(&tmp);

    if (is_http)
        *is_http = av_strstart(proto_name, "http", NULL);

    return ret;

}

static char *replace_template_str(const char *url, const char *marker)
{
    char *prefix = NULL;
    char *start = 0;
    char *end = NULL;
    char *tmp_url = NULL;
    int marker_len;

    prefix = av_strdup(marker);
    marker_len = strlen(prefix) - 1;
    prefix[marker_len] = '\0';

    start = av_stristr(url, prefix);
    if (!start)
        goto finish;
    end = strchr(start + 1, '$');
    if (!end)
        goto finish;

    tmp_url = av_mallocz(MAX_URL_SIZE);
    if (!tmp_url) {
        return NULL;
    }

    av_strlcpy(tmp_url, url, start - url+ 1);
    av_strlcat(tmp_url, start + marker_len, strlen(tmp_url) + end - start - marker_len + 1);
    av_strlcat(tmp_url, end + 1, MAX_URL_SIZE);

finish:
    av_free(prefix);
    return tmp_url;
}

static char *get_content_url(xmlNodePtr *baseurl_nodes,
                             int n_baseurl_nodes,
                             xmlChar *rep_id_val,
                             xmlChar *rep_bandwidth_val,
                             xmlChar *val)
{
    int i;
    xmlChar *text;
    char *url = NULL;
    char *tmp_str = av_mallocz(MAX_URL_SIZE);
    char *tmp_str_2 = NULL;

    if (!tmp_str) {
        return NULL;
    }
    for (i = 0; i < n_baseurl_nodes; ++i) {
        if (baseurl_nodes[i] &&
            baseurl_nodes[i]->children &&
            baseurl_nodes[i]->children->type == XML_TEXT_NODE) {
            text = xmlNodeGetContent(baseurl_nodes[i]->children);
            if (text) {
                tmp_str_2 = av_mallocz(MAX_URL_SIZE);
                if (!tmp_str_2) {
                    av_free(tmp_str);
                    return NULL;
                }
                ff_make_absolute_url(tmp_str_2, MAX_URL_SIZE, tmp_str, text);
                av_free(tmp_str);
                tmp_str = tmp_str_2;
                xmlFree(text);
            }
        }
    }
    if (val)
        av_strlcat(tmp_str, (const char*)val, MAX_URL_SIZE);

    if (rep_id_val) {
        url = av_strreplace(tmp_str, "$RepresentationID$", (const char*)rep_id_val);
        av_free(tmp_str);
        tmp_str = url;
    }
    if (rep_bandwidth_val && tmp_str)
        url = av_strreplace(tmp_str, "$Bandwidth$", (const char*)rep_bandwidth_val);
    if (tmp_str != url)
        av_free(tmp_str);
    return url;
}

static xmlChar *get_val_from_nodes_tab(xmlNodePtr *nodes, const int n_nodes, const xmlChar *attrname)
{
    int i;
    xmlChar *val;

    for (i = 0; i < n_nodes; ++i) {
        if (nodes[i]) {
            val = xmlGetProp(nodes[i], attrname);
            if (val)
                return val;
        }
    }

    return NULL;
}

static xmlNodePtr find_child_node_by_name(xmlNodePtr rootnode, const xmlChar *nodename)
{
    xmlNodePtr node = rootnode;
    if (!node) {
        return NULL;
    }

    node = xmlFirstElementChild(node);
    while (node) {
        if (!xmlStrcmp(node->name, nodename)) {
            return node;
        }
        node = xmlNextElementSibling(node);
    }
    return NULL;
}

static void check_full_number(struct representation *rep)
{
    char *temp_string = NULL;

    if (av_stristr(rep->url_template, "$Number$")) {
        temp_string = av_strreplace(rep->url_template, "$Number$", "$Number%" PRId64 "$");
        av_free(rep->url_template);
        rep->url_template = temp_string;
    }

    if (av_stristr(rep->url_template, "$Time$")) {
        temp_string = av_strreplace(rep->url_template, "$Time$", "$Time%" PRId64 "$");
        av_free(rep->url_template);
        rep->url_template = temp_string;
    }
}

static enum AVMediaType get_content_type(xmlNodePtr node)
{
    enum AVMediaType type = AVMEDIA_TYPE_UNKNOWN;
    int i = 0;
    const char *attr;

    if (node) {
        while (type == AVMEDIA_TYPE_UNKNOWN && i < 2) {
            attr = (i) ? "mimeType" : "contentType";
            xmlChar *val = xmlGetProp(node, attr);
            if (val) {
                if (av_stristr((const char *)val, "video")) {
                    type = AVMEDIA_TYPE_VIDEO;
                } else if (av_stristr((const char *)val, "audio")) {
                    type = AVMEDIA_TYPE_AUDIO;
                }
                xmlFree(val);
            }
            i++;
        }
    }
    return type;
}

static int parse_manifest_segmenturlnode(AVFormatContext *s, struct representation *rep,
                                         xmlNodePtr fragmenturl_node,
                                         xmlNodePtr *baseurl_nodes,
                                         xmlChar *rep_id_val,
                                         xmlChar *rep_bandwidth_val)
{
    xmlChar *initialization_val = NULL;
    xmlChar *media_val = NULL;

    if (!xmlStrcmp(fragmenturl_node->name, (const xmlChar *)"Initialization")) {
        initialization_val = xmlGetProp(fragmenturl_node, "sourceURL");
        if (initialization_val) {
            rep->init_section = av_mallocz(sizeof(struct fragment));
            if (!rep->init_section) {
                xmlFree(initialization_val);
                return AVERROR(ENOMEM);
            }
            rep->init_section->url = get_content_url(baseurl_nodes, 4,
                                                     rep_id_val,
                                                     rep_bandwidth_val,
                                                     initialization_val);
            if (!rep->init_section->url) {
                av_free(rep->init_section);
                xmlFree(initialization_val);
                return AVERROR(ENOMEM);
            }
            rep->init_section->size = -1;
            xmlFree(initialization_val);
        }
    } else if (!xmlStrcmp(fragmenturl_node->name, (const xmlChar *)"SegmentURL")) {
        media_val = xmlGetProp(fragmenturl_node, "media");
        if (media_val) {
            struct fragment *seg = av_mallocz(sizeof(struct fragment));
            if (!seg) {
                xmlFree(media_val);
                return AVERROR(ENOMEM);
            }
            seg->url = get_content_url(baseurl_nodes, 4,
                                       rep_id_val,
                                       rep_bandwidth_val,
                                       media_val);
            if (!seg->url) {
                av_free(seg);
                xmlFree(media_val);
                return AVERROR(ENOMEM);
            }
            seg->size = -1;
            dynarray_add(&rep->fragments, &rep->n_fragments, seg);
            xmlFree(media_val);
        }
    }

    return 0;
}

static int parse_manifest_segmenttimeline(AVFormatContext *s, struct representation *rep,
                                          xmlNodePtr fragment_timeline_node)
{
    xmlAttrPtr attr = NULL;
    xmlChar *val  = NULL;

    if (!xmlStrcmp(fragment_timeline_node->name, (const xmlChar *)"S")) {
        struct timeline *tml = av_mallocz(sizeof(struct timeline));
        if (!tml) {
            return AVERROR(ENOMEM);
        }
        attr = fragment_timeline_node->properties;
        while (attr) {
            val = xmlGetProp(fragment_timeline_node, attr->name);

            if (!val) {
                av_log(s, AV_LOG_WARNING, "parse_manifest_segmenttimeline attr->name = %s val is NULL\n", attr->name);
                continue;
            }

            if (!xmlStrcmp(attr->name, (const xmlChar *)"t")) {
                tml->t = (int64_t)strtoll(val, NULL, 10);
            } else if (!xmlStrcmp(attr->name, (const xmlChar *)"r")) {
                tml->r =(int64_t) strtoll(val, NULL, 10);
            } else if (!xmlStrcmp(attr->name, (const xmlChar *)"d")) {
                tml->d = (int64_t)strtoll(val, NULL, 10);
                rep->fragment_duration = (int64_t) strtoll(val, NULL, 10);
            }
            attr = attr->next;
            xmlFree(val);
        }
        dynarray_add(&rep->timelines, &rep->n_timelines, tml);
    }

    return 0;
}

static int parse_manifest_representation(AVFormatContext *s, const char *url,
                                         xmlNodePtr node,
                                         xmlNodePtr adaptionset_node,
                                         xmlNodePtr mpd_baseurl_node,
                                         xmlNodePtr period_baseurl_node,
                                         xmlNodePtr fragment_template_node,
                                         xmlNodePtr content_component_node,
                                         xmlNodePtr adaptionset_baseurl_node)
{
    int32_t ret = 0;
    int32_t audio_rep_idx = 0;
    int32_t video_rep_idx = 0;
    char *temp_string = NULL;
    DASHContext *c = s->priv_data;
    struct representation *rep = NULL;
    struct fragment *seg = NULL;
    xmlNodePtr representation_segmenttemplate_node = NULL;
    xmlNodePtr representation_baseurl_node = NULL;
    xmlNodePtr representation_segmentlist_node = NULL;
    xmlNodePtr fragment_timeline_node = NULL;
    xmlNodePtr fragment_templates_tab[2];
    xmlChar *duration_val = NULL;
    xmlChar *startnumber_val = NULL;
    xmlChar *timescale_val = NULL;
    xmlChar *initialization_val = NULL;
    xmlChar *media_val = NULL;
    xmlNodePtr baseurl_nodes[4];
    xmlNodePtr representation_node = node;
    xmlChar *rep_id_val = xmlGetProp(representation_node, "id");
    xmlChar *rep_bandwidth_val = xmlGetProp(representation_node, "bandwidth");
    xmlChar *rep_codecs_val = xmlGetProp(representation_node, "codecs");
    xmlChar *rep_height_val = xmlGetProp(representation_node, "height");
    xmlChar *rep_width_val = xmlGetProp(representation_node, "width");
    xmlChar *rep_frameRate_val = xmlGetProp(representation_node, "frameRate");
    xmlChar *rep_scanType_val = xmlGetProp(representation_node, "scanType");
    enum AVMediaType type = AVMEDIA_TYPE_UNKNOWN;

    // try get information from representation
    if (type == AVMEDIA_TYPE_UNKNOWN)
        type = get_content_type(representation_node);
    // try get information from contentComponen
    if (type == AVMEDIA_TYPE_UNKNOWN)
        type = get_content_type(content_component_node);
    // try get information from adaption set
    if (type == AVMEDIA_TYPE_UNKNOWN)
        type = get_content_type(adaptionset_node);
    if (type == AVMEDIA_TYPE_UNKNOWN) {
        av_log(s, AV_LOG_VERBOSE, "Parsing '%s' - skipp not supported representation type\n", url);
    //} else if ((type == AVMEDIA_TYPE_VIDEO && !c->cur_video) || (type == AVMEDIA_TYPE_AUDIO && !c->cur_audio)) {
	} else if ((type == AVMEDIA_TYPE_VIDEO) || (type == AVMEDIA_TYPE_AUDIO)) {
        // convert selected representation to our internal struct

        char temp_rep_id[MAX_FIELD_LEN];
        strcpy(temp_rep_id, rep_id_val);
        for (int i_rep = 0; i_rep < c->nb_representations; i_rep++){
          if (strcmp(c->representations[i_rep]->id, temp_rep_id) == 0){ // Representation already exists (Just a reload)
			            av_log(NULL, AV_LOG_ERROR, "Representation Already Exists\n");
                  rep = c->representations[i_rep];
          }
        }
		if (!rep) // New Representation
			rep = av_mallocz(sizeof(struct representation));

        //rep = av_mallocz(sizeof(struct representation));
        if (!rep) {
            ret = AVERROR(ENOMEM);
            goto end;
        }

        #ifdef TESTING
        av_log(NULL, AV_LOG_ERROR, "rep(%s,%s,%s,%s,%s,%s,%s)\n", (char *)rep_id_val, (char *)rep_codecs_val, (char *)rep_height_val, (char *)rep_width_val, (char *)rep_frameRate_val, (char *)rep_scanType_val, (char *)rep_bandwidth_val);
        #endif //TESTING
        //rep->id[] = "";
        if (rep_id_val)
			strcpy(rep->id, rep_id_val);
			//rep->id[0] = rep_id_val;
		//rep->codecs[] = "";
		if (rep_codecs_val)
			strcpy(rep->codecs, rep_codecs_val);
			//rep->codecs[0] = rep_codecs_val;
		rep->height = 0;
		if (rep_height_val)
			rep->height = strtol((char *)rep_height_val, NULL, 0);
		rep->width = 0;
		if (rep_width_val)
			rep->width = strtol((char *)rep_width_val, NULL, 0);
		rep->frameRate = 0;
		if (rep_frameRate_val)
			rep->frameRate = strtol((char *)rep_frameRate_val, NULL, 0);
		//rep->scanType[] = "";
		if (rep_scanType_val)
			strcpy(rep->scanType, rep_scanType_val);
			//rep->scanType[0] = rep_scanType_val;
		rep->bandwidth = 0;
		if (rep_bandwidth_val)
			rep->bandwidth = strtol((char *)rep_bandwidth_val, NULL, 0);
		#ifdef TESTING
		av_log(NULL, AV_LOG_ERROR, "rep(%s,%s,%d,%d,%d,%s,%d)\n", rep->id, rep->codecs, rep->height, rep->width, rep->frameRate, rep->scanType, rep->bandwidth);
		#endif //TESTING



        representation_segmenttemplate_node = find_child_node_by_name(representation_node, "SegmentTemplate");
        representation_baseurl_node = find_child_node_by_name(representation_node, "BaseURL");
        representation_segmentlist_node = find_child_node_by_name(representation_node, "SegmentList");
        reset_packet(&rep->pkt);

        baseurl_nodes[0] = mpd_baseurl_node;
        baseurl_nodes[1] = period_baseurl_node;
        baseurl_nodes[2] = adaptionset_baseurl_node;
        baseurl_nodes[3] = representation_baseurl_node;

        if (representation_segmenttemplate_node || fragment_template_node) {
            fragment_timeline_node = NULL;
            fragment_templates_tab[0] = representation_segmenttemplate_node;
            fragment_templates_tab[1] = fragment_template_node;

            duration_val = get_val_from_nodes_tab(fragment_templates_tab, 2, "duration");
            startnumber_val = get_val_from_nodes_tab(fragment_templates_tab, 2, "startNumber");
            timescale_val = get_val_from_nodes_tab(fragment_templates_tab, 2, "timescale");
            initialization_val = get_val_from_nodes_tab(fragment_templates_tab, 2, "initialization");
            media_val = get_val_from_nodes_tab(fragment_templates_tab, 2, "media");

            if (initialization_val) {
                rep->init_section = av_mallocz(sizeof(struct fragment));
                if (!rep->init_section) {
                    av_free(rep);
                    ret = AVERROR(ENOMEM);
                    goto end;
                }
                rep->init_section->url = get_content_url(baseurl_nodes, 4,
                                                         rep_id_val,
                                                         rep_bandwidth_val,
                                                         initialization_val);
                if (!rep->init_section->url) {
                    av_free(rep->init_section);
                    av_free(rep);
                    ret = AVERROR(ENOMEM);
                    goto end;
                }
                rep->init_section->size = -1;
                xmlFree(initialization_val);
            }

            if (media_val) {
                rep->url_template = get_content_url(baseurl_nodes, 4,
                                                    rep_id_val,
                                                    rep_bandwidth_val,
                                                    media_val);
                temp_string = rep->url_template;
                if (temp_string) {
                    if (av_stristr(temp_string, "$Number")) {
                        rep->tmp_url_type = TMP_URL_TYPE_NUMBER;  /* Number-Based. */
                        check_full_number(rep);
                    } else if (av_stristr(temp_string, "$Time")) {
                        rep->tmp_url_type = TMP_URL_TYPE_TIME; /* Time-Based. */
                        #ifdef TESTING
                        printf("rep->url_template: %s\n", rep->url_template);
                        #endif //TESTING
                        check_full_number(rep);
                        #ifdef TESTING
                        printf("rep->url_template: %s\n", rep->url_template);
                        #endif //TESTING
                    } else {
                        temp_string = NULL;
                    }
                }
                xmlFree(media_val);
            }
            if (duration_val) {
                rep->fragment_duration = (int64_t) strtoll(duration_val, NULL, 10);
                xmlFree(duration_val);
            }
            if (timescale_val) {
                rep->fragment_timescale = (int64_t) strtoll(timescale_val, NULL, 10);
                xmlFree(timescale_val);
            }
            if (startnumber_val) {
                rep->first_seq_no = (int64_t) strtoll(startnumber_val, NULL, 10);
                xmlFree(startnumber_val);
            }

            fragment_timeline_node = find_child_node_by_name(representation_segmenttemplate_node, "SegmentTimeline");

            if (!fragment_timeline_node)
                fragment_timeline_node = find_child_node_by_name(fragment_template_node, "SegmentTimeline");
            if (fragment_timeline_node) {

				// On reload, if the new MPD contain segmetntimeline node, clean up the existing
				// timeline by set the array size counter to 0.
				if (rep->n_timelines && c->is_live){
					rep->n_timelines = 0;
				}

                fragment_timeline_node = xmlFirstElementChild(fragment_timeline_node);
                while (fragment_timeline_node) {
                    ret = parse_manifest_segmenttimeline(s, rep, fragment_timeline_node);
                    if (ret < 0) {
                        return ret;
                    }
                    fragment_timeline_node = xmlNextElementSibling(fragment_timeline_node);
                }
            }
        } else if (representation_baseurl_node && !representation_segmentlist_node) {
            seg = av_mallocz(sizeof(struct fragment));
            if (!seg) {
                ret = AVERROR(ENOMEM);
                goto end;
            }
            seg->url = get_content_url(baseurl_nodes, 4, rep_id_val, rep_bandwidth_val, NULL);
            if (!seg->url) {
                av_free(seg);
                ret = AVERROR(ENOMEM);
                goto end;
            }
            seg->size = -1;
            dynarray_add(&rep->fragments, &rep->n_fragments, seg);
        } else if (!representation_baseurl_node && representation_segmentlist_node) {
            // TODO: https://www.brendanlong.com/the-structure-of-an-mpeg-dash-mpd.html
            // http://www-itec.uni-klu.ac.at/dash/ddash/mpdGenerator.php?fragmentlength=15&type=full
            xmlNodePtr fragmenturl_node = NULL;
            duration_val = xmlGetProp(representation_segmentlist_node, "duration");
            timescale_val = xmlGetProp(representation_segmentlist_node, "timescale");
            if (duration_val) {
                rep->fragment_duration = (int64_t) strtoll(duration_val, NULL, 10);
                xmlFree(duration_val);
            }
            if (timescale_val) {
                rep->fragment_timescale = (int64_t) strtoll(timescale_val, NULL, 10);
                xmlFree(timescale_val);
            }
            fragmenturl_node = xmlFirstElementChild(representation_segmentlist_node);
            while (fragmenturl_node) {
                ret = parse_manifest_segmenturlnode(s, rep, fragmenturl_node,
                                                    baseurl_nodes,
                                                    rep_id_val,
                                                    rep_bandwidth_val);
                if (ret < 0) {
                    return ret;
                }
                fragmenturl_node = xmlNextElementSibling(fragmenturl_node);
            }
        } else {
            free_representation(rep);
            rep = NULL;
            av_log(s, AV_LOG_ERROR, "Unknown format of Representation node id[%s] \n", (const char *)rep_id_val);
        }

        if (rep) {
            if (rep->fragment_duration > 0 && !rep->fragment_timescale)
                rep->fragment_timescale = 1;
            if (type == AVMEDIA_TYPE_VIDEO) {
                rep->rep_idx = video_rep_idx;
                //c->cur_video = rep;
                if (!c->cur_video){
					// assign this temporary 'rep' to cur_video only when
					// cur_video is uninitialized.
					// otherwise, rep and curvideo should point to the
					// the same address
					c->cur_video = rep;
				}
            } else {
                rep->rep_idx = audio_rep_idx;
                //c->cur_audio = rep;
                if (!c->cur_audio){
					// assign this temporary 'rep' to cur_audio only when
					// cur_audio is uninitialized.
					// otherwise, rep and curvideo should point to the
					// the same address
					c->cur_audio = rep;
				}
            }
            #ifdef TESTING
            av_log(NULL, AV_LOG_ERROR, "Adding One Representation\n");
            #endif
            dynarray_add(&c->representations, &c->nb_representations, rep);
        }
    }

    video_rep_idx += type == AVMEDIA_TYPE_VIDEO;
    audio_rep_idx += type == AVMEDIA_TYPE_AUDIO;

end:
    if (rep_id_val)
        xmlFree(rep_id_val);
    if (rep_bandwidth_val)
        xmlFree(rep_bandwidth_val);

    return ret;
}

static int parse_manifest_adaptationset(AVFormatContext *s, const char *url,
                                        xmlNodePtr adaptionset_node,
                                        xmlNodePtr mpd_baseurl_node,
                                        xmlNodePtr period_baseurl_node, int repIndex)
{
    int ret = 0;
    xmlNodePtr fragment_template_node = NULL;
    xmlNodePtr content_component_node = NULL;
    xmlNodePtr adaptionset_baseurl_node = NULL;
    xmlNodePtr node = NULL;

    node = xmlFirstElementChild(adaptionset_node);
    int nb_representation = 0;
    while (node) {
        if (!xmlStrcmp(node->name, (const xmlChar *)"SegmentTemplate")) {
            fragment_template_node = node;
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"ContentComponent")) {
            content_component_node = node;
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"BaseURL")) {
            adaptionset_baseurl_node = node;
        } else if (!xmlStrcmp(node->name, (const xmlChar *)"Representation")) {
			#ifdef TESTING
			av_log(NULL, AV_LOG_ERROR, "Representation: %d\n", ++nb_representation);
			#endif
			//av_log(NULL, AV_LOG_WARNING, "AB: Parsing Representation\n");
			//for (int i = 0; i < repIndex; i++)
			//	node = xmlNextElementSibling(node);
            ret = parse_manifest_representation(s, url, node,
                                                adaptionset_node,
                                                mpd_baseurl_node,
                                                period_baseurl_node,
                                                fragment_template_node,
                                                content_component_node,
                                                adaptionset_baseurl_node);
            if (ret < 0) {
                return ret;
            }
        }
        node = xmlNextElementSibling(node);
    }
    return 0;
}


static int parse_manifest(AVFormatContext *s, const char *url, AVIOContext *in)
{
    DASHContext *c = s->priv_data;
    int repIndex = c->rep_index;
    #ifdef TESTING
    av_log(NULL, AV_LOG_ERROR, "(repIndex, rep_index) = (%d, %d)\n", repIndex, c->rep_index);
    #endif
    int ret = 0;
    int close_in = 0;
    uint8_t *new_url = NULL;
    int64_t filesize = 0;
    char *buffer = NULL;
    AVDictionary *opts = NULL;
    xmlDoc *doc = NULL;
    xmlNodePtr root_element = NULL;
    xmlNodePtr node = NULL;
    xmlNodePtr period_node = NULL;
    xmlNodePtr mpd_baseurl_node = NULL;
    xmlNodePtr period_baseurl_node = NULL;
    xmlNodePtr adaptionset_node = NULL;
    xmlAttrPtr attr = NULL;
    xmlChar *val  = NULL;
    uint32_t perdiod_duration_sec = 0;
    uint32_t perdiod_start_sec = 0;
    int32_t audio_rep_idx = 0;
    int32_t video_rep_idx = 0;

    if (!in) {
        close_in = 1;
        /* This is XML manifest there is no need to set range header */
        av_dict_set(&opts, "seekable", "0", 0);
        // broker prior HTTP options that should be consistent across requests
        av_dict_set(&opts, "user-agent", c->user_agent, 0);
        av_dict_set(&opts, "cookies", c->cookies, 0);
        av_dict_set(&opts, "headers", c->headers, 0);

        ret = avio_open2(&in, url, AVIO_FLAG_READ, c->interrupt_callback, &opts);
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
    if (filesize <= 0) {
        filesize = 8 * 1024;
    }

    buffer = av_mallocz(filesize);
    if (!buffer) {
        return AVERROR(ENOMEM);
    }

    filesize = avio_read(in, buffer, filesize);
    if (filesize > 0) {
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
        while (attr) {
            val = xmlGetProp(node, attr->name);

            if (!xmlStrcmp(attr->name, (const xmlChar *)"availabilityStartTime")) {
                c->availability_start_time_sec = get_utc_date_time_insec(s, (const char *)val);
            } else if (!xmlStrcmp(attr->name, (const xmlChar *)"publishTime")) {
                c->publish_time_sec = get_utc_date_time_insec(s, (const char *)val);
            } else if (!xmlStrcmp(attr->name, (const xmlChar *)"minimumUpdatePeriod")) {
                c->minimum_update_period_sec = get_duration_insec(s, (const char *)val);
            } else if (!xmlStrcmp(attr->name, (const xmlChar *)"timeShiftBufferDepth")) {
                c->time_shift_buffer_depth_sec = get_duration_insec(s, (const char *)val);
            } else if (!xmlStrcmp(attr->name, (const xmlChar *)"minBufferTime")) {
                c->min_buffer_time_sec = get_duration_insec(s, (const char *)val);
            } else if (!xmlStrcmp(attr->name, (const xmlChar *)"suggestedPresentationDelay")) {
                c->suggested_presentation_delay_sec = get_duration_insec(s, (const char *)val);
            } else if (!xmlStrcmp(attr->name, (const xmlChar *)"mediaPresentationDuration")) {
                c->media_presentation_duration_sec = get_duration_insec(s, (const char *)val);
            }
            attr = attr->next;
            xmlFree(val);
        }

        mpd_baseurl_node = find_child_node_by_name(node, "BaseURL");

        // at now we can handle only one period, with the longest duration
        node = xmlFirstElementChild(node);
        int nb_periods = 0;
        while (node) {
            if (!xmlStrcmp(node->name, (const xmlChar *)"Period")) {
				#ifdef TESTING
				av_log(NULL, AV_LOG_ERROR, "Period: %d\n", ++nb_periods);
				#endif
                perdiod_duration_sec = 0;
                perdiod_start_sec = 0;
                attr = node->properties;
                while (attr) {
                    val = xmlGetProp(node, attr->name);
                    if (!xmlStrcmp(attr->name, (const xmlChar *)"duration")) {
                        perdiod_duration_sec = get_duration_insec(s, (const char *)val);
                    } else if (!xmlStrcmp(attr->name, (const xmlChar *)"start")) {
                        perdiod_start_sec = get_duration_insec(s, (const char *)val);
                    }
                    attr = attr->next;
                    xmlFree(val);
                }
                if ((perdiod_duration_sec) >= (c->period_duration_sec)) {
                    period_node = node;
                    c->period_duration_sec = perdiod_duration_sec;
                    c->period_start_sec = perdiod_start_sec;
                    if (c->period_start_sec > 0)
                        c->media_presentation_duration_sec = c->period_duration_sec;
                }
            }
            node = xmlNextElementSibling(node);
        }
        if (!period_node) {
            av_log(s, AV_LOG_ERROR, "Unable to parse '%s' - missing Period node\n", url);
            ret = AVERROR_INVALIDDATA;
            goto cleanup;
        }

        adaptionset_node = xmlFirstElementChild(period_node);
        int nb_adaptationsets = 0;
        while (adaptionset_node) {
            if (!xmlStrcmp(adaptionset_node->name, (const xmlChar *)"BaseURL")) {
                period_baseurl_node = adaptionset_node;
            } else if (!xmlStrcmp(adaptionset_node->name, (const xmlChar *)"AdaptationSet")) {
				#ifdef TESTING
				av_log(NULL, AV_LOG_ERROR, "Adaptaion Set: %d\n", ++nb_adaptationsets);
				#endif
                parse_manifest_adaptationset(s, url, adaptionset_node, mpd_baseurl_node, period_baseurl_node, repIndex);
            }
            adaptionset_node = xmlNextElementSibling(adaptionset_node);
        }
        if (c->cur_video) {
            c->cur_video->rep_count = video_rep_idx;
            c->cur_video->fix_multiple_stsd_order = 1;
            av_log(s, AV_LOG_VERBOSE, "rep_idx[%d]\n", (int)c->cur_video->rep_idx);
            av_log(s, AV_LOG_VERBOSE, "rep_count[%d]\n", (int)video_rep_idx);
        }
        if (c->cur_audio) {
            c->cur_audio->rep_count = audio_rep_idx;
        }

cleanup:
        /*free the document */
        xmlFreeDoc(doc);
        xmlCleanupParser();
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

static int64_t calc_cur_seg_no(AVFormatContext *s, struct representation *pls)
{
    DASHContext *c = pls->parent->priv_data;
    int64_t num = 0;

    if (c->is_live) {
        num = pls->first_seq_no + (((get_current_time_in_sec() - c->availability_start_time_sec) - c->presentation_delay_sec) * pls->fragment_timescale) / pls->fragment_duration;
    } else {
        num = pls->first_seq_no;
    }
    return num;
}

static int64_t calc_min_seg_no(AVFormatContext *s, struct representation *pls)
{
    DASHContext *c = pls->parent->priv_data;
    int64_t num = 0;

    if (c->is_live) {
        num = pls->first_seq_no + (((get_current_time_in_sec() - c->availability_start_time_sec) - c->time_shift_buffer_depth_sec) * pls->fragment_timescale)  / pls->fragment_duration;
    } else {
        num = pls->first_seq_no;
    }
    return num;
}

static int64_t calc_max_seg_no(AVFormatContext *s, struct representation *pls)
{
    DASHContext *c = pls->parent->priv_data;
    int64_t num = 0;
    int64_t i = 0;

    if (c->is_live) {
        num = pls->first_seq_no + (((get_current_time_in_sec() - c->availability_start_time_sec)) * pls->fragment_timescale)  / pls->fragment_duration;
        //num = pls->cur_seq_no + (((get_current_time_in_sec() - c->availability_start_time_sec)) * pls->fragment_timescale)  / pls->fragment_duration;
    } else {
        if (pls->n_fragments) {
            num = pls->first_seq_no + pls->n_fragments - 1;
        } else if (pls->n_timelines) {
            num = pls->first_seq_no + pls->n_timelines - 1;
            for (i = 0; i < pls->n_timelines; ++i) {
                num += pls->timelines[i]->r;
            }
        } else {
            num = pls->first_seq_no + (c->media_presentation_duration_sec * pls->fragment_timescale) / pls->fragment_duration;
        }
    }

    return num;
}

static int64_t get_fragment_start_time(struct representation *pls, int64_t cur_seq_no, DASHContext* c)
{
    int64_t i = 0;
    int64_t j = 0;
    int64_t num = 0;
    int64_t startTime = 0;

    if (c->is_live){
      // dirty hack to initial,
      // For unknown reason, either debug build or ffmpeg init the field
      // to 0.
      if (!pls->first_seq_no_in_representation){
      	pls->first_seq_no_in_representation = (((get_current_time_in_sec() - c->availability_start_time_sec) - c->presentation_delay_sec) * pls->fragment_timescale) / pls->fragment_duration;
      }
      num = pls->first_seq_no_in_representation;
    }

    if (pls->n_timelines) {
        for (i = 0; i < pls->n_timelines; ++i) {
            if (pls->timelines[i]->t > 0) {
                startTime = pls->timelines[i]->t;
            }
            if (num == cur_seq_no)
                goto finish;
            startTime += pls->timelines[i]->d;
            for (j = 0; j < pls->timelines[i]->r; ++j) {
                num++;
                if (num == cur_seq_no)
                    goto finish;
                startTime += pls->timelines[i]->d;
            }
            num++;
        }
    }

finish:
    return startTime;
}

static struct fragment *get_current_fragment(struct representation *pls)
{
    int64_t tmp_val = 0;
    int64_t min_seq_no = 0;
    int64_t max_seq_no = 0;
    char buffer[1024];
    char *tmp_str = NULL;
    struct fragment *seg = NULL;
    struct fragment *seg_ptr = NULL;
    DASHContext *c = pls->parent->priv_data;

    if (pls->n_fragments > 0) {
        if (pls->cur_seq_no < pls->n_fragments) {
            
            seg_ptr = pls->fragments[pls->cur_seq_no];
            seg = av_mallocz(sizeof(struct fragment));
            
            if (!seg) {
                return NULL;
            }
            
            seg->url = av_strdup(seg_ptr->url);
            if (!seg->url) {
                av_free(seg);
                return NULL;
            }
            
            seg->size = seg_ptr->size;
            seg->url_offset = seg_ptr->url_offset;
            
            return seg;
        }
    }
    if (c->is_live) {
		#ifdef TESTING
		printf("get_current_fragment (is_live)\n");
		#endif //TESTING
        while (1) {
            min_seq_no = calc_min_seg_no(pls->parent, pls);
            max_seq_no = calc_max_seg_no(pls->parent, pls);

            if (pls->cur_seq_no <= min_seq_no) {
                av_log(pls->parent, AV_LOG_VERBOSE, "old fragment: cur[%"PRId64"] min[%"PRId64"] max[%"PRId64"], playlist %d\n",
                (int64_t)pls->cur_seq_no, min_seq_no, max_seq_no, (int)pls->rep_idx);
                pls->cur_seq_no = calc_cur_seg_no(pls->parent, pls);
            //} else if (pls->cur_seq_no > max_seq_no) {
            } 

            else if ( // Hack to make both cases work @ShahzadLone for info! 
                      ( ( pls->tmp_url_type == TMP_URL_TYPE_NUMBER ) && ( pls->cur_seq_no >= max_seq_no ) ) ||
                      ( ( pls->tmp_url_type == TMP_URL_TYPE_TIME ) && ( pls->cur_seq_no > max_seq_no ) ) 
                      ) {
                av_log(pls->parent, AV_LOG_VERBOSE, "new fragment: min[%"PRId64"] max[%"PRId64"], playlist %d\n",
                min_seq_no, max_seq_no, (int)pls->rep_idx);
                av_usleep(1000);
                continue;
            } // End of Hack case

            break;
        
        } // End of while(1) loop.

        seg = av_mallocz(sizeof(struct fragment));
        if (!seg) {
            return NULL;
        }
    } else if (pls->cur_seq_no <= pls->last_seq_no) {
        seg = av_mallocz(sizeof(struct fragment));
        if (!seg) {
            return NULL;
        }
    }
    if (seg) {
        if (pls->tmp_url_type != TMP_URL_TYPE_UNSPECIFIED) {
            tmp_val = pls->tmp_url_type == TMP_URL_TYPE_NUMBER ? pls->cur_seq_no : get_fragment_start_time(pls, pls->cur_seq_no, c);
            //if (av_get_frame_filename(buffer, sizeof(buffer), pls->url_template, (int64_t)tmp_val) < 0) {
			 if (snprintf(buffer, sizeof(buffer), pls->url_template, tmp_val) < 0) {
                av_log(pls->parent, AV_LOG_ERROR, "Invalid segment filename template %s\n", pls->url_template);
                return NULL;
            }

            if (pls->tmp_url_type == TMP_URL_TYPE_NUMBER) {
                tmp_str = replace_template_str(buffer, "$Number$");
            } else if (pls->tmp_url_type == TMP_URL_TYPE_TIME) {
                tmp_str = replace_template_str(buffer, "$Time$");
            } else {
                av_log(pls->parent, AV_LOG_ERROR, "Invalid tmp_url_type\n");
                return NULL;
            }

            seg->url = av_strdup(tmp_str);
            if (!seg->url) {
                av_free(tmp_str);
                return NULL;
            }
        } else {
            av_log(pls->parent, AV_LOG_ERROR, "Unable to unable to resolve template url '%s'\n", pls->url_template);
            seg->url = av_strdup(pls->url_template);
            if (!seg->url) {
                return NULL;
            }
        }
        seg->size = -1;
    }

    av_free(tmp_str);
    return seg;
}

enum ReadFromURLMode {
    READ_NORMAL,
    READ_COMPLETE,
};

static int read_from_url(struct representation *pls, struct fragment *seg,
                         uint8_t *buf, int buf_size,
                         enum ReadFromURLMode mode)
{
    int ret;

    /* limit read if the fragment was only a part of a file */
    if (seg->size >= 0)
        buf_size = FFMIN(buf_size, pls->cur_seg_size - pls->cur_seg_offset);

    if (mode == READ_COMPLETE) {
        ret = avio_read(pls->input, buf, buf_size);
        if (ret < buf_size) {
            av_log(pls->parent, AV_LOG_WARNING, "Could not read complete fragment.\n");
        }
    } else {
        ret = avio_read(pls->input, buf, buf_size);
    }
    if (ret > 0)
        pls->cur_seg_offset += ret;

    return ret;
}

static int open_input(DASHContext *c, struct representation *pls, struct fragment *seg)
{
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
    av_log(pls->parent, AV_LOG_VERBOSE, "DASH request for url '%s', offset %"PRId64", playlist %d\n",
           url, seg->url_offset, pls->rep_idx);
    ret = open_url(pls->parent, &pls->input, url, c->avio_opts, opts, NULL);
    if (ret < 0) {
        goto cleanup;
    }

    /* Seek to the requested position. If this was a HTTP request, the offset
     * should already be where want it to, but this allows e.g. local testing
     * without a HTTP server. */
    if (!ret && seg->url_offset) {
        int64_t seekret = avio_seek(pls->input, seg->url_offset, SEEK_SET);
        if (seekret < 0) {
            av_log(pls->parent, AV_LOG_ERROR, "Unable to seek to offset %"PRId64" of DASH fragment '%s'\n", seg->url_offset, seg->url);
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
    int64_t sec_size = 0;
    int64_t urlsize = 0;
    int ret = 0;

    /* read init section only once per representation */
    if (!pls->init_section || pls->init_sec_buf) {
        return 0;
    }

    ret = open_input(c, pls, pls->init_section);
    if (ret < 0) {
        av_log(pls->parent, AV_LOG_WARNING, "Failed to open an initialization section in playlist %d\n", pls->rep_idx);
        return ret;
    }

    if (pls->init_section->size >= 0) {
        sec_size = pls->init_section->size;
    } else if ((urlsize = avio_size(pls->input)) >= 0) {
        sec_size = urlsize;
    } else {
        sec_size = max_init_section_size;
    }
    av_log(pls->parent, AV_LOG_DEBUG, "Downloading an initialization section of size %"PRId64"\n", sec_size);
    sec_size = FFMIN(sec_size, max_init_section_size);
    av_fast_malloc(&pls->init_sec_buf, &pls->init_sec_buf_size, sec_size);
    ret = read_from_url(pls, pls->init_section, pls->init_sec_buf, pls->init_sec_buf_size, READ_COMPLETE);
    ff_format_io_close(pls->parent, &pls->input);
    if (ret < 0)
        return ret;

    if (pls->fix_multiple_stsd_order && pls->rep_idx > 0) {
        uint8_t **stsd_entries = NULL;
        int *stsd_entries_size = NULL;
        int i = 4;

        while (i <= (ret - 4)) {
            // find start stsd atom
            if (!memcmp(pls->init_sec_buf + i, "stsd", 4)) {
                /* 1B version
                 * 3B flags
                 * 4B num of entries */
                int stsd_first_offset = i + 8;
                int stsd_offset = 0;
                int j = 0;
                uint32_t stsd_count = AV_RB32(pls->init_sec_buf + stsd_first_offset);
                stsd_first_offset += 4;
                if (stsd_count != pls->rep_count) {
                    i++;
                    continue;
                }
                // find all stsd entries
                stsd_entries = av_mallocz_array(stsd_count, sizeof(*stsd_entries));
                stsd_entries_size = av_mallocz_array(stsd_count, sizeof(*stsd_entries_size));
                for (j = 0; j < stsd_count; ++j) {
                    /* 4B - size
                     * 4B - format */
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
                for (j = 0; j < stsd_count; ++j) {
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
            i++;
        }
    }

    av_log(pls->parent, AV_LOG_TRACE, "pls[%p] init section size[%d]\n", pls, (int)ret);
    pls->init_sec_data_len = ret;
    pls->init_sec_buf_read_offset = 0;

    return 0;
}

static int64_t seek_data(void *opaque, int64_t offset, int whence)
{
    struct representation *v = opaque;
    if (v->n_fragments && !v->init_sec_data_len) {
        return avio_seek(v->input, offset, whence);
    }

    return AVERROR(ENOSYS);
}

static int read_data(void *opaque, uint8_t *buf, int buf_size)
{
    int ret = 0;
    struct representation *v = opaque;
    DASHContext *c = v->parent->priv_data;

restart:
    if (!v->input) {
        free_fragment(&v->cur_seg);

		// step1. get reload interval
		int64_t reload_interval = (v->n_timelines > 0) ? ( v->timelines[v->n_timelines-1]->d ) : ( v->target_duration ) ;
		//int repeat_time = v->timelines[v->n_timelines-1]->r;
		//printf("reload: %" PRId64 "\n", reload_interval);

reload:
	//		printf("\ncheck reload: curSeq:%d, first:%d rep:%d\n",
	//			v->cur_seq_no,
	//			 v->first_seq_no_in_representation,
	//			 repeat_time
	//			);


		if ( //v->type == AVMEDIA_TYPE_VIDEO &&
             //repeat_time &&
             v->first_seq_no_in_representation &&
             v->cur_seq_no - v->first_seq_no_in_representation > 10 ) {

			    AVFormatContext* fmtctx = v->parent;
			    printf("require reload-------------------------------\n");
			    if ( ( ret = parse_manifest(fmtctx, fmtctx->filename, NULL ) ) < 0 ) {
				    printf("failed: &&&&&&&&&&&& %d\n", ret);
				    return ret;
			}

			c->cur_video->first_seq_no_in_representation =
				((( get_current_time_in_sec() - c->availability_start_time_sec) - c->presentation_delay_sec) * c->cur_video->fragment_timescale) / c->cur_video->fragment_duration;
		}





        v->cur_seg = get_current_fragment(v);
        if (!v->cur_seg) {
            ret = AVERROR_EOF;
            goto end;
        }
        //printf("current segment: %s \n", v->cur_seg->url);

        /* load/update Media Initialization Section, if any */
        ret = update_init_section(v);
        if (ret)
            goto end;

		printf("open input: %s \n", v->cur_seg->url);
        ret = open_input(c, v, v->cur_seg);
        if (ret < 0) {
			printf("failed to open input: %s \n", v->cur_seg->url);
            if (ff_check_interrupt(c->interrupt_callback)) {
                goto end;
                ret = AVERROR_EXIT;
            }
            av_log(v->parent, AV_LOG_WARNING, "Failed to open fragment of playlist %d\n", v->rep_idx);
            v->cur_seq_no++;
            goto restart;
        }
    }

    if (v->init_sec_buf_read_offset < v->init_sec_data_len) {
        /* Push init section out first before first actual fragment */
        int copy_size = FFMIN(v->init_sec_data_len - v->init_sec_buf_read_offset, buf_size);
        memcpy(buf, v->init_sec_buf, copy_size);
        v->init_sec_buf_read_offset += copy_size;
        ret = copy_size;
        goto end;
    }

    /* check the v->cur_seg, if it is null, get current and double check if the new v->cur_seg*/
    if (!v->cur_seg) {
        v->cur_seg = get_current_fragment(v);
    }
    if (!v->cur_seg) {
        ret = AVERROR_EOF;
        goto end;
    }
    ret = read_from_url(v, v->cur_seg, buf, buf_size, READ_NORMAL);
    if (ret > 0)
        goto end;
    ff_format_io_close(v->parent, &v->input);
    v->cur_seq_no++;
    goto restart;

end:
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
            if (buf[0] != '\0') {
                ret = av_dict_set(&c->avio_opts, *opt, buf, AV_DICT_DONT_STRDUP_VAL);
                if (ret < 0)
                    return ret;
            }
        }
        opt++;
    }

    return ret;
}

static int nested_io_open(AVFormatContext *s, AVIOContext **pb, const char *url,
                          int flags, AVDictionary **opts)
{
    av_log(s, AV_LOG_ERROR,
           "A HDS playlist item '%s' referred to an external file '%s'. "
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
    if (!avio_ctx_buffer ) {
        ret = AVERROR(ENOMEM);
        avformat_free_context(pls->ctx);
        pls->ctx = NULL;
        goto fail;
    }
    if (c->is_live) {
        ffio_init_context(&pls->pb, avio_ctx_buffer , INITIAL_BUFFER_SIZE, 0, pls, read_data, NULL, NULL);
    } else {
        ffio_init_context(&pls->pb, avio_ctx_buffer , INITIAL_BUFFER_SIZE, 0, pls, read_data, NULL, seek_data);
    }
    pls->pb.seekable = 0;

    if ((ret = ff_copy_whiteblacklists(pls->ctx, s)) < 0)
        goto fail;

    pls->ctx->flags = AVFMT_FLAG_CUSTOM_IO;
    pls->ctx->probesize = 1024 * 4;
    pls->ctx->max_analyze_duration = 4 * AV_TIME_BASE;
    ret = av_probe_input_buffer(&pls->pb, &in_fmt, "", NULL, 0, 0);
    if (ret < 0) {
        av_log(s, AV_LOG_ERROR, "Error when loading first fragment, playlist %d\n", (int)pls->rep_idx);
        avformat_free_context(pls->ctx);
        pls->ctx = NULL;
        goto fail;
    }

    pls->ctx->pb = &pls->pb;
    pls->ctx->io_open  = nested_io_open;

    // provide additional information from mpd if available
    ret = avformat_open_input(&pls->ctx, "", in_fmt, &in_fmt_opts); //pls->init_section->url
    av_dict_free(&in_fmt_opts);
    if (ret < 0)
        goto fail;
    if (pls->n_fragments) {
        ret = avformat_find_stream_info(pls->ctx, NULL);
        if (ret < 0)
            goto fail;
    }

fail:
    return ret;
}

static int open_demux_for_component(AVFormatContext *s, struct representation *pls)
{
    int ret = 0;
    int i;

    pls->parent = s;
    pls->cur_seq_no  = calc_cur_seg_no(s, pls);
    pls->last_seq_no = calc_max_seg_no(s, pls);

    ret = reopen_demux_for_component(s, pls);
    if (ret < 0) {
        goto fail;
    }
    for (i = 0; i < pls->ctx->nb_streams; i++) {
        AVStream *st = avformat_new_stream(s, NULL);
        AVStream *ist = pls->ctx->streams[i];
        if (!st) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }
        st->id = i;
        
        if (pls->ctx->streams[i]->codecpar->format == AV_PIX_FMT_NONE)
			pls->ctx->streams[i]->codecpar->format = AV_PIX_FMT_YUV420P; //DEFAULT PIX FORMAT
		if (pls->ctx->streams[i]->codec->pix_fmt == AV_PIX_FMT_NONE)
			pls->ctx->streams[i]->codec->pix_fmt = AV_PIX_FMT_YUV420P; //DEFAULT PIX FORMAT
        avcodec_parameters_copy(st->codecpar, pls->ctx->streams[i]->codecpar);
        avcodec_copy_context(st->codec, pls->ctx->streams[i]->codec);
        #ifdef TESTING
        av_log(NULL, AV_LOG_ERROR, "st->codec->pix_fmt = %d\n", st->codec->pix_fmt);
        av_log(NULL, AV_LOG_ERROR, "st->codecpar->format = %d\n", st->codecpar->format);
        #endif //TESTING
        
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

    if ((ret = parse_manifest(s, s->filename, s->pb)) < 0)
        goto fail;

    if ((ret = save_avio_options(s)) < 0)
        goto fail;

    /* If this isn't a live stream, fill the total duration of the
     * stream. */
    if (!c->is_live) {
        s->duration = (int64_t) c->media_presentation_duration_sec * AV_TIME_BASE;
    }

    /* Open the demuxer for all video and audio components if available */
    int repIndex;
    for (repIndex = 0; repIndex < c->nb_representations; repIndex++) {
		#ifdef TESTING
		av_log(NULL, AV_LOG_ERROR, "rep[%d]->bandwidth = %d\n", repIndex, c->representations[repIndex]->bandwidth);
		#endif //TESTING
		if (!ret && c->representations[repIndex]) {
			ret = open_demux_for_component(s, c->representations[repIndex]);
			if (!ret) {
				c->representations[repIndex]->stream_index = stream_index;
				++stream_index;
			} else {
				free_representation(c->representations[repIndex]);
				c->representations[repIndex] = NULL;
			}
		}
	}

    /*
    if (!ret && c->cur_video) {
        ret = open_demux_for_component(s, c->cur_video);
        if (!ret) {
            c->cur_video->stream_index = stream_index;
            ++stream_index;
        } else {
            free_representation(c->cur_video);
            c->cur_video = NULL;
        }
    }
    */

	/*
    if (!ret && c->cur_audio) {
        ret = open_demux_for_component(s, c->cur_audio);
        if (!ret) {
            c->cur_audio->stream_index = stream_index;
            ++stream_index;
        } else {
            free_representation(c->cur_audio);
            c->cur_audio = NULL;
        }
    }
    */

    if (!stream_index) {
        ret = AVERROR_INVALIDDATA;
        goto fail;
    }

    /* Create a program */
    if (!ret) {
        AVProgram *program;
        program = av_new_program(s, 0);
        if (!program) {
            goto fail;
        }

		int repIndex;
		for (repIndex = 0; repIndex < c->nb_representations; repIndex++) {
			if (c->representations[repIndex])
				av_program_add_stream_index(s, 0, c->representations[repIndex]->stream_index);
		}

		/*
        if (c->cur_video) {
            av_program_add_stream_index(s, 0, c->cur_video->stream_index);
        }
        */

		/*
        if (c->cur_audio) {
            av_program_add_stream_index(s, 0, c->cur_audio->stream_index);
        }
        */
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
    } else if (c->cur_video->cur_timestamp < c->cur_audio->cur_timestamp) {
        cur = c->cur_video;
    } else {
        cur = c->cur_audio;
    }

    if (cur->ctx) {
        ret = av_read_frame(cur->ctx, &cur->pkt);
        if (ret < 0) {
            av_packet_unref(&cur->pkt);
        } else {
            /* If we got a packet, return it */
            *pkt = cur->pkt;
            cur->cur_timestamp = av_rescale(pkt->pts, (int64_t)cur->ctx->streams[0]->time_base.num * 90000, cur->ctx->streams[0]->time_base.den);
            pkt->stream_index = cur->stream_index;
            reset_packet(&cur->pkt);
            return 0;
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
    return 0;
}

static int dash_seek(AVFormatContext *s, struct representation *pls, int64_t seek_pos_msec, int flags)
{
    int ret = 0;
    int i = 0;
    int j = 0;
    int64_t duration = 0;

    av_log(pls->parent, AV_LOG_VERBOSE, "DASH seek pos[%"PRId64"ms], playlist %d\n", seek_pos_msec, pls->rep_idx);

    // single fragment mode
    if (pls->n_fragments == 1) {
        pls->cur_timestamp = 0;
        pls->cur_seg_offset = 0;
        ff_read_frame_flush(pls->ctx);
        return av_seek_frame(pls->ctx, -1, seek_pos_msec*1000, flags);
    }

    if (pls->input)
        ff_format_io_close(pls->parent, &pls->input);

    // find the nearest fragment
    if (pls->n_timelines > 0 && pls->fragment_timescale > 0) {
        int64_t num = pls->first_seq_no;
        av_log(pls->parent, AV_LOG_VERBOSE, "dash_seek with SegmentTimeline start n_timelines[%d] last_seq_no[%"PRId64"], playlist %d.\n",
               (int)pls->n_timelines, (int64_t)pls->last_seq_no, (int)pls->rep_idx);
        for (i = 0; i < pls->n_timelines; i++) {
            if (pls->timelines[i]->t > 0) {
                duration = pls->timelines[i]->t;
            }
            duration += pls->timelines[i]->d;
            if (seek_pos_msec < ((duration * 1000) /  pls->fragment_timescale)) {
                goto set_seq_num;
            }
            for (j = 0; j < pls->timelines[i]->r; j++) {
                duration += pls->timelines[i]->d;
                num++;
                if (seek_pos_msec < ((duration * 1000) /  pls->fragment_timescale)) {
                    goto set_seq_num;
                }
            }
            num++;
        }

set_seq_num:
        pls->cur_seq_no = num > pls->last_seq_no ? pls->last_seq_no : num;
        av_log(pls->parent, AV_LOG_VERBOSE, "dash_seek with SegmentTimeline end cur_seq_no[%"PRId64"], playlist %d.\n",
               (int64_t)pls->cur_seq_no, (int)pls->rep_idx);
    } else if (pls->fragment_duration > 0) {
        pls->cur_seq_no = pls->first_seq_no + ((seek_pos_msec * pls->fragment_timescale) / pls->fragment_duration) / 1000;
    } else {
        av_log(pls->parent, AV_LOG_ERROR, "dash_seek missing fragment_duration\n");
        pls->cur_seq_no = pls->first_seq_no;
    }
    pls->cur_timestamp = 0;
    pls->cur_seg_offset = 0;
    pls->init_sec_buf_read_offset = 0;
    ret = reopen_demux_for_component(s, pls);

    return ret;
}

static int dash_read_seek(AVFormatContext *s, int stream_index, int64_t timestamp, int flags)
{
    int ret = 0;
    DASHContext *c = s->priv_data;
    int64_t seek_pos_msec = av_rescale_rnd(timestamp, 1000,
                                           s->streams[stream_index]->time_base.den,
                                           flags & AVSEEK_FLAG_BACKWARD ?
                                           AV_ROUND_DOWN : AV_ROUND_UP);
    if ((flags & AVSEEK_FLAG_BYTE) || c->is_live)
        return AVERROR(ENOSYS);
    if (c->cur_audio) {
        ret = dash_seek(s, c->cur_audio, seek_pos_msec, flags);
    }
    if (!ret && c->cur_video) {
        ret = dash_seek(s, c->cur_video, seek_pos_msec, flags);
    }
    return ret;
}

static int dash_probe(AVProbeData *p)
{
    if (!av_stristr(p->buf, "<MPD"))
        return 0;

    if (av_stristr(p->buf, "dash:profile:isoff-on-demand:2011") ||
        av_stristr(p->buf, "dash:profile:isoff-live:2011") ||
        av_stristr(p->buf, "dash:profile:isoff-live:2012") ||
        av_stristr(p->buf, "dash:profile:isoff-main:2011")) {
        return AVPROBE_SCORE_MAX;
    }
    if (av_stristr(p->buf, "dash:profile")) {
        return AVPROBE_SCORE_MAX >> 1;
    }

    return 0;
}

#define OFFSET(x) offsetof(DASHContext, x)
#define FLAGS AV_OPT_FLAG_DECODING_PARAM
//static const AVOption dash_options[] = {
//    {NULL}
//};

static const AVOption dash_options[] = {
    { "rep_index", "representation index"  , OFFSET(rep_index), AV_OPT_TYPE_INT, { .i64 = 0   }, INT_MIN, INT_MAX, FLAGS },
    { NULL },
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
