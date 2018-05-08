/*
 * GWAVI.cpp
 *
 *
 * Copyright (c) 2008-2011, Michael Kohn
 * Copyright (c) 2013, Robin Hahling
 * Copyright (c) 2018, olegvedi@gmail.com (C++ implementation)
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the author nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "GWAVI.h"

#include <string.h>
#include <iostream>

#define ZEROIZE(x) {memset(&x, 0, sizeof(x));}

using namespace std;

/**
 * @param filename This is the name of the AVI file which will be generated by
 * this library.
 * @param width Width of a frame.
 * @param height Height of a frame.
 * @param fourcc FourCC representing the codec of the video encoded stream. a
 * FourCC is a sequence of four chars used to uniquely identify data formats.
 * For more information, you can visit www.fourcc.org.
 * @param fps Number of frames per second of your video. It needs to be > 0.
 * @param audio This parameter is optionnal. It is used for the audio track. If
 * you do not want to add an audio track to your AVI file, simply pass NULL for
 * this argument.
 *
 */
GWAVI::GWAVI(const char *filename, unsigned int width, unsigned int height, const char *fourcc, unsigned int fps,
	gwavi_audio_t *audio)
{
    ZEROIZE(avi_header);
    ZEROIZE(stream_header_v);
    ZEROIZE(stream_format_v);
    ZEROIZE(stream_header_a);
    ZEROIZE(stream_format_a);
    marker = 0;
    offsets_ptr = 0;
    offsets_len = 0;
    offsets_start = 0;
    offsets = NULL;
    offset_count = 0;

    outFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
	if (check_fourcc(fourcc) != 0)
	    (void) fprintf(stderr, "WARNING: given fourcc does not seem to "
		    "be valid: %s\n", fourcc);
	if (fps < 1)
	    throw 1;

	outFile.open(filename, ios_base::out | ios_base::trunc | ios_base::binary);

	/* set avi header */
	avi_header.time_delay = 1000000 / fps;
	avi_header.data_rate = width * height * 3;
	avi_header.flags = 0x10;

	if (audio)
	    avi_header.data_streams = 2;
	else
	    avi_header.data_streams = 1;

	/* this field gets updated when calling gwavi_close() */
	avi_header.number_of_frames = 0;
	avi_header.width = width;
	avi_header.height = height;
	avi_header.buffer_size = (width * height * 3);

	/* set stream header */
	(void) strcpy(stream_header_v.data_type, "vids");
	(void) memcpy(stream_header_v.codec, fourcc, 4);
	stream_header_v.time_scale = 1;
	stream_header_v.data_rate = fps;
	stream_header_v.buffer_size = (width * height * 3);
	stream_header_v.data_length = 0;

	/* set stream format */
	stream_format_v.header_size = 40;
	stream_format_v.width = width;
	stream_format_v.height = height;
	stream_format_v.num_planes = 1;
	stream_format_v.bits_per_pixel = 24;
	stream_format_v.compression_type = ((unsigned int) fourcc[3] << 24) + ((unsigned int) fourcc[2] << 16)
		+ ((unsigned int) fourcc[1] << 8) + ((unsigned int) fourcc[0]);
	stream_format_v.image_size = width * height * 3;
	stream_format_v.colors_used = 0;
	stream_format_v.colors_important = 0;

	stream_format_v.palette = NULL;
	stream_format_v.palette_count = 0;

	if (audio) {
	    /* set stream header */
	    memcpy(stream_header_a.data_type, "auds", 4);
	    stream_header_a.codec[0] = 1;
	    stream_header_a.codec[1] = 0;
	    stream_header_a.codec[2] = 0;
	    stream_header_a.codec[3] = 0;
	    stream_header_a.time_scale = 1;
	    stream_header_a.data_rate = audio->samples_per_second;
	    stream_header_a.buffer_size = audio->channels * (audio->bits / 8) * audio->samples_per_second;
	    /* when set to -1, drivers use default quality value */
	    stream_header_a.audio_quality = -1;
	    stream_header_a.sample_size = (audio->bits / 8) * audio->channels;

	    /* set stream format */
	    stream_format_a.format_type = 1;
	    stream_format_a.channels = audio->channels;
	    stream_format_a.sample_rate = audio->samples_per_second;
	    stream_format_a.bytes_per_second = audio->channels * (audio->bits / 8) * audio->samples_per_second;
	    stream_format_a.block_align = audio->channels * (audio->bits / 8);
	    stream_format_a.bits_per_sample = audio->bits;
	    stream_format_a.size = 0;
	}

	write_chars_bin("RIFF", 4);
	write_int(0);
	write_chars_bin("AVI ", 4);

	write_avi_header_chunk();

	write_chars_bin("LIST", 4);

	marker = outFile.tellp();

	write_int(0);
	write_chars_bin("movi", 4);

	offsets_len = 1024;
	offsets = new unsigned int[offsets_len];
	offsets_ptr = 0;

    } catch (...) {
	if (outFile.is_open()) {
	    outFile.close();
	}
	throw;
    }
}

GWAVI::~GWAVI()
{
    if (outFile.is_open()) {
	outFile.close();
    }

    delete[] offsets;
}

/**
 * This function allows you to add an encoded video frame to the AVI file.
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param buffer Video buffer size.
 * @param len Video buffer length.
 *
 * @return 0 on success, -1 on error.
 */
int GWAVI::AddVideoFrame(unsigned char *buffer, size_t len)
{
    int ret = 0;
    size_t maxi_pad; /* if your frame is raggin, give it some paddin' */
    size_t t;

    if (!buffer) {
	(void) fputs("gwavi and/or buffer argument cannot be NULL",
	stderr);
	return -1;
    }
    if (len < 256)
	(void) fprintf(stderr, "WARNING: specified buffer len seems "
		"rather small: %d. Are you sure about this?\n", (int) len);
    try {
	offset_count++;
	stream_header_v.data_length++;

	maxi_pad = len % 4;
	if (maxi_pad > 0)
	    maxi_pad = 4 - maxi_pad;

	if (offset_count >= offsets_len) {
	    offsets_len += 1024;
	    delete[] offsets;
	    offsets = new unsigned int[offsets_len];
	}

	offsets[offsets_ptr++] = (unsigned int) (len + maxi_pad);

	write_chars_bin("00dc", 4);

	write_int((unsigned int) (len + maxi_pad));

	outFile.write((char *) buffer, len);

	for (t = 0; t < maxi_pad; t++)
	    outFile.write("\0", 1);

    } catch (std::system_error& e) {
	std::cerr << e.code().message() << "\n";
	ret = -1;
    }

    return ret;
}

/**
 * This function allows you to add the audio track to your AVI file.
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param buffer Audio buffer size.
 * @param len Audio buffer length.
 *
 * @return 0 on success, -1 on error.
 */
int GWAVI::AddAudioFrame(unsigned char *buffer, size_t len)
{
    int ret = 0;
    size_t maxi_pad; /* in case audio bleeds over the 4 byte boundary  */
    size_t t;

    if (!buffer) {
	(void) fputs("gwavi and/or buffer argument cannot be NULL",
	stderr);
	return -1;
    }
    try {
	offset_count++;

	maxi_pad = len % 4;
	if (maxi_pad > 0)
	    maxi_pad = 4 - maxi_pad;

	if (offset_count >= offsets_len) {
	    offsets_len += 1024;
	    delete[] offsets;
	    offsets = new unsigned int[offsets_len];
	}

	offsets[offsets_ptr++] = (unsigned int) ((len + maxi_pad) | 0x80000000);

	write_chars_bin("01wb", 4);
	write_int((unsigned int) (len + maxi_pad));

	outFile.write((char *) buffer, len);

	for (t = 0; t < maxi_pad; t++)
	    outFile.write("\0", 1);

	stream_header_a.data_length += (unsigned int) (len + maxi_pad);

    } catch (std::system_error& e) {
	std::cerr << e.code().message() << "\n";
	ret = -1;
    }

    return ret;
}

/**
 * This function should be called when the program is done adding video and/or
 * audio frames to the AVI file. It frees memory allocated for gwavi_open() for
 * the main gwavi_t structure. It also properly closes the output file.
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 *
 * @return 0 on success, -1 on error.
 */
int GWAVI::Finalize()
{
    int ret = 0;
    long t;

    try {
	t = outFile.tellp();
	outFile.seekp(marker, ios_base::beg);
	write_int((unsigned int) (t - marker - 4));
	outFile.seekp(t, ios_base::beg);

	write_index(offset_count, offsets);

	delete[] offsets;
	offsets = NULL;

	/* reset some avi header fields */
	avi_header.number_of_frames = stream_header_v.data_length;

	t = outFile.tellp();
	outFile.seekp(12, ios_base::beg);
	write_avi_header_chunk();
	outFile.seekp(t, ios_base::beg);

	t = outFile.tellp();
	outFile.seekp(4, ios_base::beg);
	write_int((unsigned int) (t - 8));
	outFile.seekp(t, ios_base::beg);

	if (stream_format_v.palette) // TODO check
	    delete[] stream_format_v.palette;

	outFile.close();
    } catch (std::system_error& e) {
	std::cerr << e.code().message() << "\n";
	ret = -1;
    }

    return ret;
}

/**
 * This function allows you to reset the framerate. In a standard use case, you
 * should not need to call it. However, if you need to, you can call it to reset
 * the framerate after you are done adding frames to your AVI file and before
 * you call gwavi_close().
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param fps Number of frames per second of your video.
 *
 * @return 0 on success, -1 on error.
 */
void GWAVI::SetFramerate(unsigned int fps)
{
    stream_header_v.data_rate = fps;
    avi_header.time_delay = (10000000 / fps);
}

/**
 * This function allows you to reset the video codec. In a standard use case,
 * you should not need to call it. However, if you need to, you can call it to
 * reset the video codec after you are done adding frames to your AVI file and
 * before you call gwavi_close().
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param fourcc FourCC representing the codec of the video encoded stream. a
 *
 * @return 0 on success, -1 on error.
 */
void GWAVI::SetFourccCodec(const char *fourcc)
{
    if (check_fourcc(fourcc) != 0) {
	(void) fprintf(stderr, "WARNING: given fourcc does not seem to "
		"be valid: %s\n", fourcc);
    }

    memcpy(stream_header_v.codec, fourcc, 4);
    stream_format_v.compression_type = ((unsigned int) fourcc[3] << 24) + ((unsigned int) fourcc[2] << 16)
	    + ((unsigned int) fourcc[1] << 8) + ((unsigned int) fourcc[0]);
}

/**
 * This function allows you to reset the video size. In a standard use case, you
 * should not need to call it. However, if you need to, you can call it to reset
 * the video height and width set in the AVI file after you are done adding
 * frames to your AVI file and before you call gwavi_close().
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param width Width of a frame.
 * @param height Height of a frame.
 *
 * @return 0 on success, -1 on error.
 */
void GWAVI::SetVideoFrameSize(unsigned int width, unsigned int height)
{
    unsigned int size = (width * height * 3);

    avi_header.data_rate = size;
    avi_header.width = width;
    avi_header.height = height;
    avi_header.buffer_size = size;
    stream_header_v.buffer_size = size;
    stream_format_v.width = width;
    stream_format_v.height = height;
    stream_format_v.image_size = size;

}

void GWAVI::write_avi_header(struct gwavi_header_t *avi_header)
{
    long marker, t;

    write_chars_bin("avih", 4);
    marker = outFile.tellp();
    write_int(0);
    write_int(avi_header->time_delay);
    write_int(avi_header->data_rate);
    write_int(avi_header->reserved);
    /* dwFlags */
    write_int(avi_header->flags);
    /* dwTotalFrames */
    write_int(avi_header->number_of_frames);
    write_int(avi_header->initial_frames);
    write_int(avi_header->data_streams);
    write_int(avi_header->buffer_size);
    write_int(avi_header->width);
    write_int(avi_header->height);
    write_int(avi_header->time_scale);
    write_int(avi_header->playback_data_rate);
    write_int(avi_header->starting_time);
    write_int(avi_header->data_length);

    t = outFile.tellp();
    outFile.seekp(marker, ios_base::beg);

    write_int((unsigned int) (t - marker - 4));
    outFile.seekp(t, ios_base::beg);
}

void GWAVI::write_stream_header(struct gwavi_stream_header_t *stream_header)
{
    long marker, t;

    write_chars_bin("strh", 4);
    marker = outFile.tellp();
    write_int(0);

    write_chars_bin(stream_header->data_type, 4);
    write_chars_bin(stream_header->codec, 4);
    write_int(stream_header->flags);
    write_int(stream_header->priority);
    write_int(stream_header->initial_frames);
    write_int(stream_header->time_scale);
    write_int(stream_header->data_rate);
    write_int(stream_header->start_time);
    write_int(stream_header->data_length);
    write_int(stream_header->buffer_size);
    write_int(stream_header->video_quality);
    write_int(stream_header->sample_size);
    write_int(0);
    write_int(0);

    t = outFile.tellp();
    outFile.seekp(marker, ios_base::beg);
    write_int((unsigned int) (t - marker - 4));
    outFile.seekp(t, ios_base::beg);
}

void GWAVI::write_stream_format_v(struct gwavi_stream_format_v_t *stream_format_v)
{
    long marker, t;
    unsigned int i;

    write_chars_bin("strf", 4);
    marker = outFile.tellp();
    write_int(0);
    write_int(stream_format_v->header_size);
    write_int(stream_format_v->width);
    write_int(stream_format_v->height);
    write_short(stream_format_v->num_planes);
    write_short(stream_format_v->bits_per_pixel);
    write_int(stream_format_v->compression_type);
    write_int(stream_format_v->image_size);
    write_int(stream_format_v->x_pels_per_meter);
    write_int(stream_format_v->y_pels_per_meter);
    write_int(stream_format_v->colors_used);
    write_int(stream_format_v->colors_important);

    if (stream_format_v->colors_used != 0) {
	for (i = 0; i < stream_format_v->colors_used; i++) {
	    unsigned char c = stream_format_v->palette[i] & 255;
	    outFile.write((char *) &c, 1);
	    c = (stream_format_v->palette[i] >> 8) & 255;
	    outFile.write((char *) &c, 1);
	    c = (stream_format_v->palette[i] >> 16) & 255;
	    outFile.write((char *) &c, 1);
	    outFile.write("\0", 1);
	}
    }

    t = outFile.tellp();
    outFile.seekp(marker, ios_base::beg);
    write_int((unsigned int) (t - marker - 4));
    outFile.seekp(t, ios_base::beg);
}

void GWAVI::write_stream_format_a(struct gwavi_stream_format_a_t *stream_format_a)
{
    long marker, t;

    write_chars_bin("strf", 4);
    marker = outFile.tellp();
    write_int(0);
    write_short(stream_format_a->format_type);
    write_short(stream_format_a->channels);
    write_int(stream_format_a->sample_rate);
    write_int(stream_format_a->bytes_per_second);
    write_short(stream_format_a->block_align);
    write_short(stream_format_a->bits_per_sample);
    write_short(stream_format_a->size);

    t = outFile.tellp();
    outFile.seekp(marker, ios_base::beg);
    write_int((unsigned int) (t - marker - 4));
    outFile.seekp(t, ios_base::beg);
}

void GWAVI::write_avi_header_chunk()
{
    long marker, t;
    long sub_marker;

    write_chars_bin("LIST", 4);
    marker = outFile.tellp();
    write_int(0);
    write_chars_bin("hdrl", 4);
    write_avi_header(&avi_header);

    write_chars_bin("LIST", 4);
    sub_marker = outFile.tellp();
    write_int(0);
    write_chars_bin("strl", 4);
    write_stream_header(&stream_header_v);
    write_stream_format_v(&stream_format_v);

    t = outFile.tellp();

    outFile.seekp(sub_marker, ios_base::beg);
    write_int((unsigned int) (t - sub_marker - 4));
    outFile.seekp(t, ios_base::beg);

    if (avi_header.data_streams == 2) {
	write_chars_bin("LIST", 4);
	sub_marker = outFile.tellp();
	write_int(0);
	write_chars_bin("strl", 4);
	write_stream_header(&stream_header_a);
	write_stream_format_a(&stream_format_a);

	t = outFile.tellp();
	outFile.seekp(sub_marker, ios_base::beg);
	write_int((unsigned int) (t - sub_marker - 4));
	outFile.seekp(t, ios_base::beg);
    }

    t = outFile.tellp();
    outFile.seekp(marker, ios_base::beg);
    write_int((unsigned int) (t - marker - 4));
    outFile.seekp(t, ios_base::beg);
}

void GWAVI::write_index(int count, unsigned int *offsets)
{
    long marker, t;
    unsigned int offset = 4;

    if (!offsets)
	throw 1;

    write_chars_bin("idx1", 4);
    marker = outFile.tellp();
    write_int(0);

    for (t = 0; t < count; t++) {
	if ((offsets[t] & 0x80000000) == 0)
	    write_chars("00dc");
	else {
	    write_chars("01wb");
	    offsets[t] &= 0x7fffffff;
	}
	write_int(0x10);
	write_int(offset);
	write_int(offsets[t]);

	offset = offset + offsets[t] + 8;
    }

    t = outFile.tellp();
    outFile.seekp(marker, ios_base::beg);
    write_int((unsigned int) (t - marker - 4));
    outFile.seekp(t, ios_base::beg);

}

/**
 * Return 0 if fourcc is valid, 1 non-valid or -1 in case of errors.
 */
int GWAVI::check_fourcc(const char *fourcc)
{
    int ret = 0;
    /* list of fourccs from http://fourcc.org/codecs.php */
    const char valid_fourcc[] = "3IV1 3IV2 8BPS"
	    "AASC ABYR ADV1 ADVJ AEMI AFLC AFLI AJPG AMPG ANIM AP41 ASLC"
	    "ASV1 ASV2 ASVX AUR2 AURA AVC1 AVRN"
	    "BA81 BINK BLZ0 BT20 BTCV BW10 BYR1 BYR2"
	    "CC12 CDVC CFCC CGDI CHAM CJPG CMYK CPLA CRAM CSCD CTRX CVID"
	    "CWLT CXY1 CXY2 CYUV CYUY"
	    "D261 D263 DAVC DCL1 DCL2 DCL3 DCL4 DCL5 DIV3 DIV4 DIV5 DIVX"
	    "DM4V DMB1 DMB2 DMK2 DSVD DUCK DV25 DV50 DVAN DVCS DVE2 DVH1"
	    "DVHD DVSD DVSL DVX1 DVX2 DVX3 DX50 DXGM DXTC DXTN"
	    "EKQ0 ELK0 EM2V ES07 ESCP ETV1 ETV2 ETVC"
	    "FFV1 FLJP FMP4 FMVC FPS1 FRWA FRWD FVF1"
	    "GEOX GJPG GLZW GPEG GWLT"
	    "H260 H261 H262 H263 H264 H265 H266 H267 H268 H269"
	    "HDYC HFYU HMCR HMRR"
	    "I263 ICLB IGOR IJPG ILVC ILVR IPDV IR21 IRAW ISME"
	    "IV30 IV31 IV32 IV33 IV34 IV35 IV36 IV37 IV38 IV39 IV40 IV41"
	    "IV41 IV43 IV44 IV45 IV46 IV47 IV48 IV49 IV50"
	    "JBYR JPEG JPGL"
	    "KMVC"
	    "L261 L263 LBYR LCMW LCW2 LEAD LGRY LJ11 LJ22 LJ2K LJ44 LJPG"
	    "LMP2 LMP4 LSVC LSVM LSVX LZO1"
	    "M261 M263 M4CC M4S2 MC12 MCAM MJ2C MJPG MMES MP2A MP2T MP2V"
	    "MP42 MP43 MP4A MP4S MP4T MP4V MPEG MPNG MPG4 MPGI MR16 MRCA MRLE"
	    "MSVC MSZH"
	    "MTX1 MTX2 MTX3 MTX4 MTX5 MTX6 MTX7 MTX8 MTX9"
	    "MVI1 MVI2 MWV1"
	    "NAVI NDSC NDSM NDSP NDSS NDXC NDXH NDXP NDXS NHVU NTN1 NTN2"
	    "NVDS NVHS"
	    "NVS0 NVS1 NVS2 NVS3 NVS4 NVS5"
	    "NVT0 NVT1 NVT2 NVT3 NVT4 NVT5"
	    "PDVC PGVV PHMO PIM1 PIM2 PIMJ PIXL PJPG PVEZ PVMM PVW2"
	    "QPEG QPEQ"
	    "RGBT RLE RLE4 RLE8 RMP4 RPZA RT21 RV20 RV30 RV40 S422 SAN3"
	    "SDCC SEDG SFMC SMP4 SMSC SMSD SMSV SP40 SP44 SP54 SPIG SQZ2"
	    "STVA STVB STVC STVX STVY SV10 SVQ1 SVQ3"
	    "TLMS TLST TM20 TM2X TMIC TMOT TR20 TSCC TV10 TVJP TVMJ TY0N"
	    "TY2C TY2N"
	    "UCOD ULTI"
	    "V210 V261 V655 VCR1 VCR2 VCR3 VCR4 VCR5 VCR6 VCR7 VCR8 VCR9"
	    "VDCT VDOM VDTZ VGPX VIDS VIFP VIVO VIXL VLV1 VP30 VP31 VP40"
	    "VP50 VP60 VP61 VP62 VP70 VP80 VQC1 VQC2 VQJC VSSV VUUU VX1K"
	    "VX2K VXSP VYU9 VYUY"
	    "WBVC WHAM WINX WJPG WMV1 WMV2 WMV3 WMVA WNV1 WVC1"
	    "X263 X264 XLV0 XMPG XVID"
	    "XWV0 XWV1 XWV2 XWV3 XWV4 XWV5 XWV6 XWV7 XWV8 XWV9"
	    "XXAN"
	    "Y16 Y411 Y41P Y444 Y8 YC12 YUV8 YUV9 YUVP YUY2 YUYV YV12 YV16"
	    "YV92"
	    "ZLIB ZMBV ZPEG ZYGO ZYYY";

    if (!fourcc) {
	(void) fputs("fourcc cannot be NULL", stderr);
	return -1;
    }
    if (strchr(fourcc, ' ') || !strstr(valid_fourcc, fourcc))
	ret = 1;

    return ret;
}

void GWAVI::write_int(unsigned int n)
{
    unsigned char buffer[4];

    buffer[0] = n;
    buffer[1] = n >> 8;
    buffer[2] = n >> 16;
    buffer[3] = n >> 24;

    outFile.write((char *) buffer, 4);
}

void GWAVI::write_short(unsigned int n)
{
    unsigned char buffer[2];

    buffer[0] = n;
    buffer[1] = n >> 8;

    outFile.write((char *) buffer, 2);
}

void GWAVI::write_chars(const char *s)
{
    int count = strlen(s);
    if (count > 255)
	count = 255;
    outFile.write(s, count);
}

void GWAVI::write_chars_bin(const char *s, int count)
{
    outFile.write(s, count);
}

