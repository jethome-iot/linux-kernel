/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef MEDIAPROXY_H
#define MEDIAPROXY_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/kfifo.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/amlogic/media/media_proxy/AmlVideoUserdata.h>
#include <linux/amlogic/media/media_proxy/AmlMediaErrorCodes.h>

#define DEV_NAME "mediaproxy"
//TODO：debug sys
#define STR_MAX_SIZE  32
#define KFIFO_MAX_SIZE  32 /*the number of elements in the fifo, this must be a power of 2*/
#define MAX_SESSION 16

#define READ_TIME_OUT   100   /* Time to wait for read in millisecond */

#define MEDIAPROXY_MAGIC 'm'
#define MEDIAPROXY_CONNECT _IOW(MEDIAPROXY_MAGIC, 0, int)
#define MEDIAPROXY_DISCONNECT _IOW(MEDIAPROXY_MAGIC, 1, int)
#define MEDIAPROXY_GET_CONSUMER_COUNT _IOW(MEDIAPROXY_MAGIC, 2, int)
#define MEDIAPROXY_MSG_TYPE__SUBSCRIBE _IOW(MEDIAPROXY_MAGIC, 3, int)
#define MEDIAPROXY_SET_FIFO_LEN _IOW(MEDIAPROXY_MAGIC, 4, int)

#define MP_ROLE_STRING(role) \
	(((role) == MP_ROLE_PRODUCER) ? "producer" : \
	"consumer")

enum mp_role_e {
	MP_ROLE_INVALID,
	MP_ROLE_PRODUCER,
	MP_ROLE_CONSUMER,
};

/*state change*/
enum mp_fifo_state_e {
	MP_FIFO_NONE,
	MP_FIFO_UNUSING,
	MP_FIFO_USED,
	MP_FIFO_UNUSED,
};

union mediaproxy_ioctl_args {
	enum mp_role_e role;
	u32 subscribe_msg_type;
	u32 fifo_len;
};

struct mediaproxy_fifo {
	u32 subscribe_msg_type;
	enum mp_fifo_state_e state;
	char module_name[STR_MAX_SIZE];
	DECLARE_KFIFO_PTR(msg_kfifo, struct aml_video_user_data);
};

/*
 * struct mediaproxy_session the session for connect
 * session_id index of the mp_session array
 * role  producer or consumer
 * subscribe_msg_type
 * Type mask for subscription messages, currently available only to consumers
 * lock
 * a pointer to the p_lock/c_lock within the dev structure
 * session_entry
 * a pointer to the producers/consumers session array within the dev
 * msg_kfifo
 * the FIFO in the session is used to store message data
 */
struct mediaproxy_session {
	int session_id;
	enum mp_role_e role;
	u32 subscribe_msg_type;
	char *module_name;
	struct mutex *lock;/* a pointer to the p_lock/c_lock. */
	struct mediaproxy_session **session_entry;
	int fifo_idx;
	int fifo_len;
	DECLARE_KFIFO_PTR(msg_kfifo, struct aml_video_user_data);
};

/*
 * struct mediaproxy_dev
 *the metadata of the mediaproxy device node
 *@major:
 *the major device number of mediaproxy dev
 *@has_consumer
 *the consumer count
 *@all_producer_fifo_empty
 *used to determine if there is data in the producer's fifo
 *@producers:
 *array for Storing Producer Sessions
 *@consumers:
 *array for Storing Consumers Sessions
 *@p_lock
 *@c_lock
 *@read_queue
 *consumer wait queue for message
 *@transfer_queue
 *wait queue fot transfer thread (mediaproxy_thread)
 */
struct mediaproxy_dev {
	int major;
	unsigned int has_consumer;
	bool all_producer_fifo_empty;
	int has_unusing_fifo;
	u32 subscribe_msg_type;
	struct mediaproxy_session *producers[MAX_SESSION];
	struct mediaproxy_session *consumers[MAX_SESSION];
	struct mediaproxy_fifo *p_fifo[MAX_SESSION];
	struct mediaproxy_fifo *c_fifo[MAX_SESSION];
	struct mutex p_lock;/* mutex lock for producer. */
	struct mutex c_lock;/* mutex lock for consumer. */
	wait_queue_head_t read_queue;
	wait_queue_head_t transfer_queue;
};

static int _check_thread_wakeup(void);
static int mediaproxy_thread_fn(void *data);
/*
 *Copy msg from the producer fifo to the consumer fifo,
 *depending on the type of message the consumer is subscribed to.
 */
static int copy_data_between_kfifo(void);

// ioctl
static int mediaproxy_connect(enum mp_role_e role,
							u32 fifo_len,
							struct mediaproxy_session *session);
static int mediaproxy_disconnect(struct mediaproxy_session *session);
static int mediaproxy_get_consumer_count(void);
static ssize_t session_kernel_write(struct mediaproxy_session *session, int num, void *buf);
static int session_creat(void **sess, char *name);
static int producer_thread_fn(void *data);
static int consumer_thread_fn(void *data);
int notify_msg_to_mediaproxy(void *handle, int num, void *data);
int media_proxy_produce_deinit(void *handle);
int receive_msg_from_mediaproxy(void *handle,  int num, void *data);
//extern struct file_operations mediaproxy_fops;

#endif
