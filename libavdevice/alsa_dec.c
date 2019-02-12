/*
 * ALSA input and output
 * Copyright (c) 2007 Luca Abeni ( lucabe72 email it )
 * Copyright (c) 2007 Benoit Fouet ( benoit fouet free fr )
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

/**
 * @file
 * ALSA input and output: input
 * @author Luca Abeni ( lucabe72 email it )
 * @author Benoit Fouet ( benoit fouet free fr )
 * @author Nicolas George ( nicolas george normalesup org )
 *
 * This avdevice decoder can capture audio from an ALSA (Advanced
 * Linux Sound Architecture) device.
 *
 * The filename parameter is the name of an ALSA PCM device capable of
 * capture, for example "default" or "plughw:1"; see the ALSA documentation
 * for naming conventions. The empty string is equivalent to "default".
 *
 * The capture period is set to the lower value available for the device,
 * which gives a low latency suitable for real-time capture.
 *
 * The PTS are an Unix time in microsecond.
 *
 * Due to a bug in the ALSA library
 * (https://bugtrack.alsa-project.org/alsa-bug/view.php?id=4308), this
 * decoder does not work with certain ALSA plugins, especially the dsnoop
 * plugin.
 */

#include <alsa/asoundlib.h>

#include "libavutil/internal.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/time.h"

#include "libavformat/internal.h"
#include "libavformat/spdif.h"
#include "libavformat/avio_internal.h"

#include "avdevice.h"
#include "alsa.h"

static int write_spdif_packet(AlsaData *s, uint8_t *buf, int buf_size)
{
    SpdifData *spdif = &s->spdif;

    av_log(s, AV_LOG_WARNING, "Write: Amount:%d,Used Space:%d,Free Space:%d\n", buf_size, av_fifo_size(spdif->buffer), av_fifo_space(spdif->buffer));

    if (0 == av_fifo_space(spdif->buffer)) {
        return AVERROR(ENOMEM);
    }

    buf_size = FFMIN(buf_size, av_fifo_space(spdif->buffer));

    av_fifo_generic_write(spdif->buffer, buf, buf_size, NULL);

    return buf_size;
}

static void set_spdif(AVFormatContext *s, SpdifData *spdif, AVPacket* pkt)
{
    enum AVCodecID codec;
    int len = 1<<16;
    int ret = ffio_ensure_seekback(s->pb, len);
    AlsaData *alsa = s->priv_data;
    if (ret >= 0) {
        uint8_t *buf = av_malloc(len);
        av_log(s, AV_LOG_WARNING, "SPDIF writing %d bytes\n", pkt->size);
        write_spdif_packet(alsa, pkt->data, pkt->size);
        //av_log(s, AV_LOG_WARNING, "\n 0 | ");
        //for (int i = 0; i < pkt->size; i++) {
        //    av_log(s, AV_LOG_WARNING, "%02x ", pkt->data[i]);
        //    if (i && (i % 16) == 0) {
        //        av_log(s, AV_LOG_WARNING, "\n %d | ", i);
        //    }
        //}
        //av_log(s, AV_LOG_WARNING, "\n");
        if (!buf) {
            ret = AVERROR(ENOMEM);
        } else {
            int64_t pos = avio_tell(s->pb);
            av_log(s, AV_LOG_WARNING, "SPDIF buffer at position:%"PRIi64", len:%d\n", pos, len);
            len = ret = avio_read(s->pb, buf, len);
            av_log(s, AV_LOG_WARNING, "SPDIF Read %d\n", len);
            if (len >= 0) {
                //av_log(s, AV_LOG_WARNING, "\n 0 | ");
                //for (int i = 0; i < len; i++) {
                //    av_log(s, AV_LOG_WARNING, "%02x ", buf[i]);
                //    if (i && (i % 16) == 0) {
                //        av_log(s, AV_LOG_WARNING, "\n %d | ", i);
                //    }
                //}
                //av_log(s, AV_LOG_WARNING, "\n");
                ret = ff_spdif_probe(buf, len, &codec);
                if (ret > AVPROBE_SCORE_EXTENSION) {
                    s->streams[0]->codecpar->codec_id = codec;
                    spdif->enabled = 1;
                    av_log(s, AV_LOG_WARNING, "Setting SPDIF to format %d\n", codec);
                }
                else {
                    av_log(s, AV_LOG_WARNING, "SPDIF check returns %d vs %d\n", ret, AVPROBE_SCORE_EXTENSION);
                    //av_log(s, AV_LOG_WARNING, "\n 0 | ");
                    //for (int i = 0; i < len; i++) {
                    //    av_log(s, AV_LOG_WARNING, "%02x ", buf[i]);
                    //    if (i && (i % 16) == 0) {
                    //        av_log(s, AV_LOG_WARNING, "\n %d | ", i);
                    //    }
                    //}
                    //av_log(s, AV_LOG_WARNING, "\n");
                }
                if (AVPROBE_SCORE_EXTENSION == ret) {
                    exit(-1);
                }
            }
            else {
                av_log(s, AV_LOG_WARNING, "SPDIF Failed reading %d\n", len);
            }
            avio_seek(s->pb, pos, SEEK_SET);
            av_free(buf);
        }
    }

    if (ret < 0)
        av_log(s, AV_LOG_WARNING, "Cannot check for SPDIF\n");
}

static int read_spdif_packet(void *opaque, uint8_t *buf, int buf_size)
{
    AlsaData *s = (AlsaData *)opaque;
    SpdifData *spdif = &s->spdif;

    buf_size = FFMIN(buf_size, av_fifo_size(spdif->buffer) - spdif->read_len);

    if (!buf_size)
        return AVERROR_EOF;

    av_log(s, AV_LOG_WARNING, "Read Packet\n");
    av_log(s, AV_LOG_WARNING, "Output buffer:%p,size:%d\n", buf, buf_size);
    av_log(s, AV_LOG_WARNING, "Input buffer:%p size:%d,buffer read:%d\n", spdif->buffer->rptr, av_fifo_size(spdif->buffer), spdif->read_len);
    // LOOK AT ME HERE, MULTIPLE READS ARE NOT BEING POPULATED INTO THE BUFFER PROPERLY
    av_fifo_generic_peek(spdif->buffer, buf+spdif->read_len, buf_size, NULL);
    spdif->read_len += buf_size;

    return buf_size;
}

static int64_t seek(void *opaque, int64_t offset, int whence)
{
    AlsaData *s = (AlsaData *)opaque;
    SpdifData *spdif = &s->spdif;

    spdif->read_len = offset;
    return 0;
}

static av_cold int audio_read_header(AVFormatContext *s1)
{
    AlsaData *s = s1->priv_data;
    SpdifData *spdif = &s->spdif;

    AVStream *st;
    int ret;
    enum AVCodecID codec_id;

    st = avformat_new_stream(s1, NULL);
    if (!st) {
        av_log(s1, AV_LOG_ERROR, "Cannot add stream\n");

        return AVERROR(ENOMEM);
    }
    codec_id    = s1->audio_codec_id;

    ret = ff_alsa_open(s1, SND_PCM_STREAM_CAPTURE, &s->sample_rate, s->channels,
        &codec_id);
    if (ret < 0) {
        return AVERROR(EIO);
    }

    /* take real parameters */
    st->codecpar->codec_type  = AVMEDIA_TYPE_AUDIO;
    st->codecpar->codec_id    = codec_id;
    st->codecpar->sample_rate = s->sample_rate;
    st->codecpar->channels    = s->channels;
    st->codecpar->frame_size = s->frame_size;
    avpriv_set_pts_info(st, 64, 1, 1000000);  /* 64 bits pts in us */
    /* microseconds instead of seconds, MHz instead of Hz */
    s->timefilter = ff_timefilter_new(1000000.0 / s->sample_rate,
                                      s->period_size, 1.5E-6);
    if (!s->timefilter)
        goto fail;

    if (CONFIG_SPDIF_DEMUXER) {
        AVIOContext *avio_ctx = NULL;
        //spdif->buffer = av_fifo_alloc(1<<16);
        //spdif->buffer = av_fifo_alloc(ALSA_BUFFER_SIZE_MAX);
        spdif->avio_ctx_buffer_size = 4096;
        //spdif->avio_ctx_buffer_size = ALSA_BUFFER_SIZE_MAX;
        spdif->avio_ctx_buffer = av_malloc(spdif->avio_ctx_buffer_size);
        spdif->buffer = av_fifo_alloc(ALSA_BUFFER_SIZE_MAX);
        // Initialize read length
        spdif->read_len = 0;
        avio_ctx = avio_alloc_context(spdif->avio_ctx_buffer, spdif->avio_ctx_buffer_size,
            AVIO_FLAG_READ, s, &read_spdif_packet, NULL, &seek);
        s1->pb = avio_ctx;
        av_log(s, AV_LOG_WARNING, "Alloc'd spdif buffer %p in %p\n", spdif->buffer, spdif);
    }

    return 0;

fail:
    snd_pcm_close(s->h);
    return AVERROR(EIO);
}

static int audio_read_packet(AVFormatContext *s1, AVPacket *pkt)
{
    AlsaData *s  = s1->priv_data;
    SpdifData *spdif  = &s->spdif;
    int res;
    int64_t dts;
    snd_pcm_sframes_t delay = 0;

    if (av_new_packet(pkt, s->period_size * s->frame_size) < 0) {
        return AVERROR(EIO);
    }

    while ((res = snd_pcm_readi(s->h, pkt->data, s->period_size)) < 0) {
        if (res == -EAGAIN) {
            av_packet_unref(pkt);

            return AVERROR(EAGAIN);
        }
        if (ff_alsa_xrun_recover(s1, res) < 0) {
            av_log(s1, AV_LOG_ERROR, "ALSA read error: %s\n",
                   snd_strerror(res));
            av_packet_unref(pkt);

            return AVERROR(EIO);
        }
        ff_timefilter_reset(s->timefilter);
    }

    if (CONFIG_SPDIF_DEMUXER) {
        if (spdif->enabled == 1) {
            // Copy it to pb
            av_log(s, AV_LOG_WARNING, "Copying SPDIF to packet %d\n", pkt->size);
            av_packet_unref(pkt);

            // Do logic to update buffer state (copy buffers, etc)
            int ret = ff_spdif_read_packet(s1, pkt);
            av_fifo_drain(spdif->buffer, spdif->read_len);
            spdif->read_len = 0;

            return ret;
        }
        else {
            set_spdif(s1, spdif, pkt);
        }
    }

    dts = av_gettime();
    snd_pcm_delay(s->h, &delay);
    dts -= av_rescale(delay + res, 1000000, s->sample_rate);
    pkt->pts = ff_timefilter_update(s->timefilter, dts, s->last_period);
    s->last_period = res;

    pkt->size = res * s->frame_size;


    return 0;
}

static int audio_get_device_list(AVFormatContext *h, AVDeviceInfoList *device_list)
{
    return ff_alsa_get_device_list(device_list, SND_PCM_STREAM_CAPTURE);
}

static const AVOption options[] = {
    { "sample_rate", "", offsetof(AlsaData, sample_rate), AV_OPT_TYPE_INT, {.i64 = 48000}, 1, INT_MAX, AV_OPT_FLAG_DECODING_PARAM },
    { "channels",    "", offsetof(AlsaData, channels),    AV_OPT_TYPE_INT, {.i64 = 2},     1, INT_MAX, AV_OPT_FLAG_DECODING_PARAM },
    { NULL },
};

static const AVClass alsa_demuxer_class = {
    .class_name     = "ALSA demuxer",
    .item_name      = av_default_item_name,
    .option         = options,
    .version        = LIBAVUTIL_VERSION_INT,
    .category       = AV_CLASS_CATEGORY_DEVICE_AUDIO_INPUT,
};

AVInputFormat ff_alsa_demuxer = {
    .name           = "alsa",
    .long_name      = NULL_IF_CONFIG_SMALL("ALSA audio input"),
    .priv_data_size = sizeof(AlsaData),
    .read_header    = audio_read_header,
    .read_packet    = audio_read_packet,
    .read_close     = ff_alsa_close,
    .get_device_list = audio_get_device_list,
    .flags          = AVFMT_NOFILE,
    .priv_class     = &alsa_demuxer_class,
};
