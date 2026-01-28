/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef AML_VIDEO_USER_DATA_H_
#define AML_VIDEO_USER_DATA_H_

#define MAX_AFD_LEN (64)
enum media_message_type {
	MEDIA_MESSAGE_TYPE_INVALID    = 0,
	MEDIA_VDEC_CONNECTED          = (1 << 0),  // 0001
	MEDIA_VIDEO_FRIST_OUT_FRAME_INFO   = (1 << 1),// 0010
	MEDIA_VIDEO_INPUT_INFO       = (1 << 2),  // 0100
	MEDIA_VIDEO_OUTPUT_INFO      = (1 << 3),  // 1000
	MEDIA_VIDEO_STATISTIC_INFO   = (1 << 4),  // 10000
	MEDIA_VIDEO_ERROR_EVENT      = (1 << 5),   // 100000
	MEDIA_VIDEO_METRICS_FRAME_DECODED_INFO    = (1 << 6),	// 1000000
	MEDIA_VIDEO_METRICS_FRAME_TOGGLE_INFO     = (1 << 7),	// 10000000
	MEDIA_VIDEO_METRICS_FRAME_SIGNAFENCE_INFO = (1 << 8)	// 10000 0000
};

enum video_block_type {
	READ_DATA_BLOCK,
	READ_DATA_NON_BLOCK,
};

struct resolution {
	u32 width;
	u32 height;
};

struct vdec_hdr_info {
	u32 color_primaries;
	u32 matrix_coeff;
};

struct video_first_frame_info {
	u32 decoder_id;   /* id of current instance */
	struct resolution res;
	struct vdec_hdr_info hdr;
	char afd_data[MAX_AFD_LEN];
};

struct vdec_connected_info {
	u32 decoder_id;
	u32 vdec_connect;
};

struct video_input_buf_info {
	u32 decoder_id;
	u32 bitstream_id;
	u32 timestamp;
	u32 data_size;
};

struct video_output_info {
	u32 decoder_id;
	u64 bitstream_id;
	u64 timestamp;
	u32 picture_buffer_id;
};

struct video_statistics_info {
	u32 decoder_id;
	u32 decoded_frames;
	u32 err_frames;
	u32 drop_frames;
};

struct video_error_info {
	u32 error_event;
};

struct frame_msg_info {
	s32 decoder_instid;
	s32 vd_instid;
	s64 frame_index;
	s64 ino;
	s64 bitstreamid;
	s64 time;
	s64 drop;
	s64 reserved[10];
};

struct aml_video_user_data {
	u32 message_type;
	u32 data_size;
	union {
	struct vdec_connected_info conn_info;
	struct video_first_frame_info first_frame_info;
	struct video_input_buf_info input_info;
	struct video_output_info output_info;
	struct video_statistics_info statistics_info;
	struct video_error_info  error_info;
	struct frame_msg_info frame_info;
	} data;
};

#endif
