// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "media_proxy.h"
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>

static struct mediaproxy_dev *mediaproxy;
static struct device *mediaproxy_device;
static struct task_struct *mediaproxy_thread;
static struct task_struct *vdec_thread;
static struct task_struct *di_thread;

static struct mediaproxy_session *k_dec_producer_session;
static struct mediaproxy_session *k_di_producer_session;
static struct mediaproxy_session *k_dec_cus_session;

MODULE_PARM_DESC(mediaproxy_debug, "\n\t\t mediaproxy debug");
static int mediaproxy_debug;
module_param(mediaproxy_debug, int, 0664);

#define pr_dbg(args...)\
do {\
	if (mediaproxy_debug)\
		pr_info(args);\
} while (0)

static int mediaproxy_init(void)
{
	int ret = 0;

	pr_info("mediaproxy init\n");
	mediaproxy = kzalloc(sizeof(*mediaproxy), GFP_KERNEL);
	if (!mediaproxy) {
		pr_err("Failed to malloc mediaproxy_dev\n");
		return ret;
	}
	mutex_init(&mediaproxy->p_lock);
	mutex_init(&mediaproxy->c_lock);
	init_waitqueue_head(&mediaproxy->read_queue);
	init_waitqueue_head(&mediaproxy->transfer_queue);
	mediaproxy->has_consumer = 0;
	mediaproxy->all_producer_fifo_empty = true;
	mediaproxy->has_unusing_fifo = 0;
	mediaproxy->subscribe_msg_type = 0;
	return ret;
}

static void mediaproxy_cleanup(void)
{
	int i;

	if (mediaproxy) {
		for (i = 0; i < MAX_SESSION; i++) {
			if (mediaproxy->producers[i]) {
				pr_info("%s free producers[%d]\n", __func__, i);
				//had alloc kfifo, need free fifo
				if (mediaproxy->producers[i]->session_id >= 0)
					kfifo_free(&mediaproxy->producers[i]->msg_kfifo);
				kfree(mediaproxy->producers[i]);
				mediaproxy->producers[i] = NULL;
			}
			if (mediaproxy->consumers[i]) {
				pr_info("%s free consumers[%d]\n", __func__, i);
				kfree(mediaproxy->consumers[i]);
				mediaproxy->consumers[i] = NULL;
			}
			if (mediaproxy->p_fifo[i]) {
				pr_info("%s free p_fifo[%d]\n", __func__, i);
				if (mediaproxy->p_fifo[i]->state != MP_FIFO_NONE)
					kfifo_free(&mediaproxy->p_fifo[i]->msg_kfifo);
				kfree(mediaproxy->p_fifo[i]);
				mediaproxy->p_fifo[i] = NULL;
			}
			if (mediaproxy->c_fifo[i]) {
				pr_info("%s free c_fifo[%d]\n", __func__, i);
				if (mediaproxy->c_fifo[i]->state != MP_FIFO_NONE)
					kfifo_free(&mediaproxy->c_fifo[i]->msg_kfifo);
				kfree(mediaproxy->c_fifo[i]);
				mediaproxy->c_fifo[i] = NULL;
			}
		}
		kfree(mediaproxy);
		mediaproxy = NULL;
	}
}

static int mediaproxy_get_fifo(enum mp_role_e type, int size)
{
	int idx = -1;
	int i;
	int ret = 0;
	int alloced_idx = -1;

	if (!mediaproxy) {
		pr_err("Failed to get kfifo mediaproxy isnull\n");
		return idx;
	}
	//get fifo from alloced fifo
	for (i = 0; i < MAX_SESSION; i++) {
		if (type == MP_ROLE_PRODUCER) {
			if (mediaproxy->p_fifo[i]) {
				if (mediaproxy->p_fifo[i]->state == MP_FIFO_UNUSED) {
					alloced_idx = i;
					if (kfifo_size(&mediaproxy->p_fifo[i]->msg_kfifo) >= size)
						idx = i;
				}
			} else {
				//alloc new file
				mediaproxy->p_fifo[i] =
						kzalloc(sizeof(struct mediaproxy_fifo), GFP_KERNEL);
				ret = kfifo_alloc(&mediaproxy->p_fifo[i]->msg_kfifo,
						KFIFO_MAX_SIZE, GFP_KERNEL);
				if (ret) {
					pr_err("Failed to alloc kfifo size:[%d]\n", size);
					return idx;
				}
				mediaproxy->p_fifo[i]->state = MP_FIFO_USED;
				pr_info("success to alloc kfifo size:[%d] i=[%d]\n", size, i);
				idx = i;
			}
			//if not get fifo.we need realloc fifo
			if (idx == -1 && alloced_idx != -1) {
				//
				kfifo_free(&mediaproxy->p_fifo[alloced_idx]->msg_kfifo);
				ret = kfifo_alloc(&mediaproxy->p_fifo[alloced_idx]->msg_kfifo,
								KFIFO_MAX_SIZE, GFP_KERNEL);
				if (ret) {
					pr_err("Failed to realloc kfifo size:[%d]\n", size);
					return idx;
				}
				mediaproxy->p_fifo[alloced_idx]->state = MP_FIFO_USED;
				idx = alloced_idx;
				pr_info("find p realloc kfifo size:[%d][%d]\n", size, idx);
				break;
			}
			if (idx >= 0) {
				pr_info("find p kfifo size:[%d][%d]\n", size, idx);
				//reset fifo
				mediaproxy->p_fifo[idx]->state = MP_FIFO_USED;
				break;
			}

		} else {
			if (mediaproxy->c_fifo[i]) {
				if (mediaproxy->c_fifo[i]->state == MP_FIFO_UNUSED) {
					alloced_idx = i;
					if (kfifo_size(&mediaproxy->c_fifo[i]->msg_kfifo) >= size)
						idx = i;
				}
			} else {
				//alloc new file
				mediaproxy->c_fifo[i] =
						kzalloc(sizeof(struct mediaproxy_fifo), GFP_KERNEL);
				ret = kfifo_alloc(&mediaproxy->c_fifo[i]->msg_kfifo,
								KFIFO_MAX_SIZE, GFP_KERNEL);
				if (ret) {
					pr_err("Failed to alloc kfifo size:[%d]\n", size);
					return idx;
				}
				mediaproxy->c_fifo[i]->state = MP_FIFO_USED;
				pr_info("success to alloc kfifo size:[%d] i=[%d]\n", size, i);
				idx = i;
			}
			//if not get fifo.we need realloc fifo
			if (idx == -1 && alloced_idx != -1) {
				//
				kfifo_free(&mediaproxy->c_fifo[alloced_idx]->msg_kfifo);
				ret = kfifo_alloc(&mediaproxy->c_fifo[alloced_idx]->msg_kfifo,
									KFIFO_MAX_SIZE, GFP_KERNEL);
				if (ret) {
					pr_err("Failed to realloc kfifo size:[%d]\n", size);
					return idx;
				}
				mediaproxy->c_fifo[alloced_idx]->state = MP_FIFO_USED;
				idx = alloced_idx;
				pr_info("find c realloc kfifo size:[%d][%d]\n", size, idx);
				break;
			}
			if (idx >= 0) {
				pr_info("find c kfifo size:[%d][%d]\n", size, idx);
				mediaproxy->c_fifo[idx]->state = MP_FIFO_USED;
				break;
			}
		}
	}
	pr_info("%s get idx[%d]\n", __func__, idx);
	return idx;
}

static int session_creat(void **sess, char *name)
{
	struct mediaproxy_session *session;

	pr_info("Mediaproxy is opened name[%s]\\n", name);
	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session) {
		pr_info("kzalloc session error\n");
		return -1;
	}
	session->session_id = -1;
	session->role = MP_ROLE_INVALID;
	session->subscribe_msg_type = 0x0;
	session->module_name = name;
	session->lock = NULL;
	session->session_entry = NULL;
	session->fifo_idx = -1;
	session->fifo_len = KFIFO_MAX_SIZE;
	*sess = session;
	pr_info("Mediaproxy is opened name[%s] end\\n", name);
	return 0;
}

static ssize_t session_kernel_write(struct mediaproxy_session *session, int num, void *buf)
{
	unsigned int copied = num;
	int idx;

	if (!mediaproxy || !session) {
		pr_err("Mediaproxy or session is not initialized\n");
		return -EFAULT;
	}
	idx = session->fifo_idx;
	if (idx == -1) {
		pr_err("session fifo idx -1. not connect\n");
		return -EFAULT;
	}
	if (session->role != MP_ROLE_PRODUCER)
		return -EFAULT;

	kfifo_in(&mediaproxy->p_fifo[idx]->msg_kfifo, buf, copied);
	mediaproxy->all_producer_fifo_empty = kfifo_is_empty(&mediaproxy->p_fifo[idx]->msg_kfifo);
	wake_up_interruptible(&mediaproxy->transfer_queue);
	pr_dbg("mediaproxy write success, module[%s] copied size: %d fifo len: %d\n",
		session->module_name,
		copied, kfifo_len(&mediaproxy->p_fifo[idx]->msg_kfifo));
	return copied;
}

int media_error_event_notify(int event)
{
	struct aml_video_user_data msg;

	memset(&msg, 0, sizeof(struct aml_video_user_data));
	msg.message_type = MEDIA_VIDEO_ERROR_EVENT;
	msg.data.error_info.error_event = event;
	return 0;
}
EXPORT_SYMBOL(media_error_event_notify);

static int producer_thread_fn(void *user_data)
{
	int event = MEDIA_ERRORCODES_PIPELINE_MIN;
	int i = 0;
	struct aml_video_user_data data[10] = {0};

	while (!kthread_should_stop()) {
		msleep(3000);
		memset(&data[0], 0, sizeof(struct aml_video_user_data));
		data[0].message_type = MEDIA_VIDEO_ERROR_EVENT;
		data[0].data.error_info.error_event = event;
		event++;
		if (event >= MEDIA_ERRORCODES_PIPELINE_MAX)
			event = MEDIA_ERRORCODES_PIPELINE_MIN;
		memset(&data[1], 0, sizeof(struct aml_video_user_data));
		data[1].message_type = MEDIA_VIDEO_STATISTIC_INFO;
		data[1].data.statistics_info.decoder_id = 1;
		data[1].data.statistics_info.decoded_frames = i;
		data[1].data.statistics_info.err_frames = 1;
		data[1].data.statistics_info.drop_frames = 1;

		memset(&data[2], 0, sizeof(struct aml_video_user_data));
		data[2].message_type = MEDIA_VIDEO_ERROR_EVENT;
		data[2].data.error_info.error_event = event;
		event++;
		if (event >= MEDIA_ERRORCODES_PIPELINE_MAX)
			event = MEDIA_ERRORCODES_PIPELINE_MIN;
		memset(&data[3], 0, sizeof(struct aml_video_user_data));
		data[3].message_type = MEDIA_VIDEO_ERROR_EVENT;
		data[3].data.error_info.error_event = event;
		event++;
		if (event >= MEDIA_ERRORCODES_PIPELINE_MAX)
			event = MEDIA_ERRORCODES_PIPELINE_MIN;
		pr_dbg("set msg type [%d]\n", data[0].message_type);
		pr_dbg("set msg type [%d]\n", data[1].message_type);
		notify_msg_to_mediaproxy(k_dec_producer_session, 2, data);
		pr_dbg("set msg type [%d]\n", data[2].message_type);
		pr_dbg("set msg type [%d]\n", data[3].message_type);
		notify_msg_to_mediaproxy(k_di_producer_session, 2, data + 2);
		i++;
		if (i > 100000)
			i = 0;
	}
	return 0;
}

static int consumer_thread_fn(void *data)
{
	struct aml_video_user_data msg[10] = {0};
	int i = 0, num = 0;

	while (!kthread_should_stop()) {
		msleep(3000);
		memset(msg, 0, 10 * sizeof(struct aml_video_user_data));
		num = receive_msg_from_mediaproxy(k_dec_cus_session, 2, msg);
		pr_info("get msg num [%d]\n", num);

		for (i = 0; i < num; i++) {
			if (msg[i].message_type == MEDIA_VIDEO_ERROR_EVENT) {
				pr_info("get msg type [%d]\n", msg[i].message_type);
				pr_info("get msg error code [%d]\n",
				msg[i].data.error_info.error_event);
			}
			if (msg[i].message_type == MEDIA_VIDEO_STATISTIC_INFO) {
				pr_info("get msg type [%d]\n",
				msg[i].message_type);
				pr_dbg("get msg id [%d]\n",
				msg[i].data.statistics_info.decoder_id);
				pr_dbg("get msg id [%d]\n",
				msg[i].data.statistics_info.decoded_frames);
				pr_dbg("get msg id [%d]\n",
				msg[i].data.statistics_info.err_frames);
				pr_dbg("get msg id [%d]\n",
				msg[i].data.statistics_info.drop_frames);
			}
		}
	}
	return 0;
}

int update_subscribe_msg_type(void)
{
	u32 subscribe_msg_type = 0;
	int i = 0;

	if (!mediaproxy)
		return 0;

	for (i = 0; i < MAX_SESSION; i++) {
		if (mediaproxy->consumers[i]) {
			subscribe_msg_type =
				subscribe_msg_type |
				mediaproxy->consumers[i]->subscribe_msg_type;
		}
	}
	mediaproxy->subscribe_msg_type = subscribe_msg_type;
	pr_info("mediaproxy update subscribe_msg_type 0x[%x]\n", mediaproxy->subscribe_msg_type);
	return 0;
}

int mediaproxy_open(struct inode *inode, struct file *filp)
{
	int ret;

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;
	pr_info("mediaproxy_open start\n");
	ret = session_creat((void **)&filp->private_data, "app");
	if (ret) {
		pr_err("Failed to creat session\n");
		return ret;
	}
	return 0;
}

int mediaproxy_release(struct inode *inode, struct file *filp)
{
	struct mediaproxy_session *session = filp->private_data;

	pr_info("release session\n");
	if (session->session_id >= 0) {
		//not disconnect, get lock
		struct mutex *lock = session->lock;

		mutex_lock(lock);
		pr_info("release session %s-%d\n",
			MP_ROLE_STRING(session->role), session->session_id);
		session->session_entry[session->session_id] = NULL;
		if (session->role == MP_ROLE_CONSUMER)
			mediaproxy->has_consumer--;
		session->subscribe_msg_type = 0x0;

		if (session->role == MP_ROLE_CONSUMER) {
			//
			mediaproxy->c_fifo[session->fifo_idx]->state = MP_FIFO_UNUSING;
			update_subscribe_msg_type();
		} else {
			//
			kfifo_free(&session->msg_kfifo);
			mediaproxy->p_fifo[session->fifo_idx]->state = MP_FIFO_UNUSING;
		}
		mediaproxy->has_unusing_fifo++;
		pr_info("release has_unusing_fifo [%d]\n", mediaproxy->has_unusing_fifo);
		kfree(session);
		filp->private_data = NULL;
		mutex_unlock(lock);
		wake_up_interruptible(&mediaproxy->transfer_queue);
	} else {
		//disconnect only need session
		kfree(session);
		filp->private_data = NULL;
	}
	module_put(THIS_MODULE);
	return 0;
}

static int mediaproxy_connect(enum mp_role_e role, u32 fifo_len, struct mediaproxy_session *session)
{
	int i, ret;

	if (!mediaproxy) {
		pr_err("Mediaproxy is not initialized\n");
		return -EFAULT;
	}
	if (session->session_id != -1) {
		pr_err("Session %s-%d is already connected\n",
			MP_ROLE_STRING(role), session->session_id);
		return -EEXIST;
	}
	switch (role) {
	case MP_ROLE_PRODUCER:
		mutex_lock(&mediaproxy->p_lock);
		for (i = 0; i < MAX_SESSION; i++) {
			if (!mediaproxy->producers[i]) {
				session->session_id = i;
				session->role = role;
				session->lock = &mediaproxy->p_lock;
				session->session_entry = mediaproxy->producers;
				mediaproxy->producers[i] = session;
				ret = kfifo_alloc(&session->msg_kfifo, fifo_len, GFP_KERNEL);
				if (ret)
					pr_err("Failed to alloc size:[%d]\n", fifo_len);
				session->fifo_idx = mediaproxy_get_fifo(role, fifo_len);
				pr_info("Session %s-%d is connected fifo idx [%d]\n",
				MP_ROLE_STRING(role), session->session_id, session->fifo_idx);
				if (session->fifo_idx >= 0) {
					mediaproxy->p_fifo[session->fifo_idx]->subscribe_msg_type =
						session->subscribe_msg_type;
					strscpy(mediaproxy->p_fifo[session->fifo_idx]->module_name,
						session->module_name, STR_MAX_SIZE - 1);
				}
				break;
			}
		}
		mutex_unlock(&mediaproxy->p_lock);
		if (session->fifo_idx == -1) {
			pr_err("producer [%s] can not got fifo\n", session->module_name);
			return -EFAULT;
		}
		return 0;
	case MP_ROLE_CONSUMER:
		mutex_lock(&mediaproxy->c_lock);
		for (i = 0; i < MAX_SESSION; i++) {
			if (!mediaproxy->consumers[i]) {
				session->session_id = i;
				session->role = role;
				session->lock = &mediaproxy->c_lock;
				session->session_entry = mediaproxy->consumers;
				mediaproxy->consumers[i] = session;
				mediaproxy->has_consumer++;
				session->fifo_idx = mediaproxy_get_fifo(role, fifo_len);
				wake_up_interruptible(&mediaproxy->transfer_queue);
				pr_info("Session %s-%d is connected fifo idx [%d]\n",
				MP_ROLE_STRING(role), session->session_id, session->fifo_idx);
				if (session->fifo_idx >= 0) {
					mediaproxy->c_fifo[session->fifo_idx]->subscribe_msg_type =
						session->subscribe_msg_type;
					strscpy(mediaproxy->c_fifo[session->fifo_idx]->module_name,
						session->module_name, STR_MAX_SIZE - 1);
				}
				break;
			}
		}
		mutex_unlock(&mediaproxy->c_lock);
		if (session->fifo_idx == -1) {
			pr_err("consumer [%s] can not got fifo\n", session->module_name);
			return -EFAULT;
		}
		return 0;
	case MP_ROLE_INVALID:
	default:
		pr_err("Invalid role\n");
		return -EINVAL;
	}
}

static int mediaproxy_disconnect(struct mediaproxy_session *session)
{
	if (!session || !session->lock) {
		pr_info("Session or lock is null when disconnected\n");
		return 0;
	}
	mutex_lock(session->lock);
	session->session_entry[session->session_id] = NULL;
	if (session->role == MP_ROLE_CONSUMER)
		mediaproxy->has_consumer--;

	pr_info("Session %s-%d is disconnected\n",
		MP_ROLE_STRING(session->role), session->session_id);
	session->session_id = -1;
	session->subscribe_msg_type = 0x0;
	session->session_entry = NULL;
	if (session->role == MP_ROLE_CONSUMER) {
		mediaproxy->c_fifo[session->fifo_idx]->state = MP_FIFO_UNUSING;
		mediaproxy->c_fifo[session->fifo_idx]->subscribe_msg_type = 0x0;
		update_subscribe_msg_type();
	} else {
		kfifo_free(&session->msg_kfifo);
		mediaproxy->p_fifo[session->fifo_idx]->state = MP_FIFO_UNUSING;
		mediaproxy->p_fifo[session->fifo_idx]->subscribe_msg_type = 0x0;
	}
	mediaproxy->has_unusing_fifo++;
	pr_info("disconnect has_unusing_fifo [%d]\n", mediaproxy->has_unusing_fifo);
	session->role = MP_ROLE_INVALID;
	session->fifo_idx = -1;
	mutex_unlock(session->lock);
	session->lock = NULL;
	wake_up_interruptible(&mediaproxy->transfer_queue);
	return 0;
}

ssize_t mediaproxy_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	unsigned int copied = 0;
	int idx = -1;
	struct mediaproxy_session *session = filp->private_data;

	if (session)
		idx = session->fifo_idx;
	if (!mediaproxy) {
		pr_err("Mediaproxy is not initialized\n");
		return -EFAULT;
	}
	if (idx < 0) {
		//pr_err("session is not connect fifo[%d]\n", idx);
		return -EFAULT;
	}
	if (!session || session->role != MP_ROLE_CONSUMER) {
		pr_err("session is not consumer\n");
		return -EFAULT;
	}

	if (kfifo_is_empty(&mediaproxy->c_fifo[idx]->msg_kfifo)) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible_timeout(mediaproxy->read_queue,
			!kfifo_is_empty(&mediaproxy->c_fifo[idx]->msg_kfifo),
			msecs_to_jiffies(READ_TIME_OUT)) == 0) {
			// timeout
			pr_dbg("read timeout\n");
			return -ETIMEDOUT;
		}
	}

	if (kfifo_to_user(&mediaproxy->c_fifo[idx]->msg_kfifo, buf, count, &copied)) {
		pr_err("consumer kfifo_to_user failed\n");
		return -EFAULT;
	}
	pr_dbg("Mediaproxy is read success, copied:%d, fifo len: %d\n",
		copied, kfifo_len(&mediaproxy->c_fifo[idx]->msg_kfifo));
	return copied;
}

ssize_t mediaproxy_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	unsigned int copied = 0;
	int idx = -1;
	struct mediaproxy_session *session = filp->private_data;
	int data_copied;
	struct aml_video_user_data msg;

	if (session)
		idx = session->fifo_idx;
	if (!mediaproxy) {
		pr_err("Mediaproxy is not initialized\n");
		return -EFAULT;
	}
	if (idx < 0) {
		//pr_err("session is not connect\n");
		return -EFAULT;
	}
	if (session->role != MP_ROLE_PRODUCER) {
		pr_err("session is not producer\n");
		return -EFAULT;
	}
	kfifo_reset_out(&session->msg_kfifo);
	if (kfifo_from_user(&session->msg_kfifo, buf, count, &copied)) {
		pr_err("producer fifo_from_user failed\n");
		return -EFAULT;
	}
	//copy from fifo to fifo
	while (!kfifo_is_empty(&session->msg_kfifo)) {
		memset(&msg, 0, sizeof(struct aml_video_user_data));
		data_copied = kfifo_out(&session->msg_kfifo, &msg, 1);
		if ((msg.message_type & mediaproxy->subscribe_msg_type)) {
			pr_dbg("mediaproxy write p [%d] [%s]",
				idx, mediaproxy->p_fifo[idx]->module_name);
			kfifo_in(&mediaproxy->p_fifo[idx]->msg_kfifo, &msg, data_copied);
		} else {
			pr_dbg("mediaproxy skip write p[%d] [%s] 0x[%x]0x[%x]-[%d] len[%d]", idx,
				mediaproxy->p_fifo[idx]->module_name,
				msg.message_type, mediaproxy->subscribe_msg_type,
				msg.message_type & mediaproxy->subscribe_msg_type,
				kfifo_len(&session->msg_kfifo));
		}
	}
	//mutex_unlock(&mediaproxy->p_lock);
	mediaproxy->all_producer_fifo_empty = kfifo_is_empty(&mediaproxy->p_fifo[idx]->msg_kfifo);
	wake_up_interruptible(&mediaproxy->transfer_queue);
	//mutex_unlock(&mediaproxy->p_lock);
	pr_dbg("mediaproxy write success, copied size: %d fifo len: %d\n",
		copied, kfifo_len(&mediaproxy->p_fifo[idx]->msg_kfifo));
	return copied;
}

static long mediaproxy_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int result = 0;
	union mediaproxy_ioctl_args args = {.subscribe_msg_type = 0};

	struct mediaproxy_session *session = filp->private_data;

	if (_IOC_SIZE(cmd) > sizeof(args)) {
		pr_err("ioctl size is too large\n");
		return -EINVAL;
	}
	if ((void *)arg && copy_from_user(&args, (void *)arg, sizeof(args))) {
		pr_err("ioctl copy from user failed\n");
		return -EFAULT;
	}
	switch (cmd) {
	case MEDIAPROXY_CONNECT:
		pr_info("MEDIAPROXY_CONNECT\n");
		result = mediaproxy_connect(args.role, session->fifo_len, session);
		break;
	case MEDIAPROXY_DISCONNECT:
		pr_info("MEDIAPROXY_DISCONNECT\n");
		result = mediaproxy_disconnect(session);
		break;
	case MEDIAPROXY_GET_CONSUMER_COUNT:
		result = mediaproxy_get_consumer_count();
		break;
	case MEDIAPROXY_MSG_TYPE__SUBSCRIBE:
		session->subscribe_msg_type = args.subscribe_msg_type;
		if (session->role == MP_ROLE_CONSUMER)
			update_subscribe_msg_type();
		if (session->fifo_idx >= 0) {
			//had connect buf not subscribe or need update msg type
			if (session->role == MP_ROLE_PRODUCER)
				mediaproxy->p_fifo[session->fifo_idx]->subscribe_msg_type =
					args.subscribe_msg_type;
			else
				mediaproxy->c_fifo[session->fifo_idx]->subscribe_msg_type =
					args.subscribe_msg_type;
		}
		pr_info("MEDIAPROXY_MSG_TYPE__SUBSCRIBE type %d [%d][%s] role [%d]\n",
			args.subscribe_msg_type, session->fifo_idx,
			session->module_name, session->role);
		break;
	case MEDIAPROXY_SET_FIFO_LEN:
		{
			if (args.fifo_len <= 32)
				session->fifo_len = 32;
			else if (args.fifo_len <= 64)
				session->fifo_len = 64;
			else if (args.fifo_len <= 128)
				session->fifo_len = 128;
			else
				session->fifo_len = 256;
			pr_info("MEDIAPROXY_SET_FIFO_LEN fifo len [%d][%d]\n",
				session->fifo_len, args.fifo_len);
		}
		break;
	default:
		pr_err("Unknown ioctl cmd\n");
		result = -EINVAL;
		break;
	}
	return result;
}

static int _check_thread_wakeup(void)
{
	int cond = 0;

	if (mediaproxy->has_consumer > 0 && !mediaproxy->all_producer_fifo_empty)
		cond = 1;
	if (mediaproxy->has_unusing_fifo > 0)
		cond = 1;
	if (kthread_should_stop())
		cond = 1;

	return cond;
}

static int mediaproxy_thread_fn(void *data)
{
	while (!kthread_should_stop()) {
		if (wait_event_interruptible(mediaproxy->transfer_queue, _check_thread_wakeup())) {
			pr_err("exit thread\n");
			return -ERESTARTSYS;
		}

		if (copy_data_between_kfifo() < 0)
			return -ERESTARTSYS;
	}

	return 0;
}

static int copy_data_between_kfifo(void)
{
	int i, j, data_copied;
	struct aml_video_user_data msg;

	memset(&msg, 0, sizeof(struct aml_video_user_data));

	for (i = 0; i < MAX_SESSION; i++) {
		if (mediaproxy->p_fifo[i] && !kfifo_is_empty(&mediaproxy->p_fifo[i]->msg_kfifo)) {
			data_copied = kfifo_out(&mediaproxy->p_fifo[i]->msg_kfifo, &msg, 1);
			mediaproxy->all_producer_fifo_empty =
				kfifo_is_empty(&mediaproxy->p_fifo[i]->msg_kfifo);
			for (j = 0; j < MAX_SESSION; j++) {
				if (!mediaproxy->c_fifo[j])
					continue;

				int type = mediaproxy->c_fifo[j]->subscribe_msg_type;

				pr_dbg("copy type: 0x%x consumer[%d]: 0x%x [%s] : %d state [%d]\n",
					msg.message_type,
					j,
					type,
					mediaproxy->c_fifo[j]->module_name,
					msg.message_type & type,
					mediaproxy->c_fifo[j]->state);
				if (mediaproxy->c_fifo[j]->state != MP_FIFO_USED) {
					//fifo had unused,we not need copy msg
					continue;
				}
				if ((msg.message_type & type)) {
					pr_dbg("mediaproxy copy c[%d] [%s]",
						j, mediaproxy->c_fifo[j]->module_name);
					kfifo_in(&mediaproxy->c_fifo[j]->msg_kfifo,
						&msg, data_copied);
				}
			}
			wake_up_interruptible(&mediaproxy->read_queue);
			memset(&msg, 0, sizeof(struct aml_video_user_data));
		}
		if (mediaproxy->p_fifo[i] &&
			mediaproxy->p_fifo[i]->state == MP_FIFO_UNUSING &&
			kfifo_is_empty(&mediaproxy->p_fifo[i]->msg_kfifo)) {
			mediaproxy->p_fifo[i]->state = MP_FIFO_UNUSED;
			mediaproxy->has_unusing_fifo--;
			pr_info("p_fifo[%d] name[%s] state[%d] to  MP_FIFO_UNUSED\n",
				i,
				mediaproxy->p_fifo[i]->module_name,
				mediaproxy->has_unusing_fifo);
		}
		if (mediaproxy->c_fifo[i] &&
			mediaproxy->c_fifo[i]->state == MP_FIFO_UNUSING) {
			//fifo had unused,we need clear fifo
			kfifo_reset_out(&mediaproxy->c_fifo[i]->msg_kfifo);
			mediaproxy->c_fifo[i]->state = MP_FIFO_UNUSED;
			mediaproxy->has_unusing_fifo--;
			pr_info("c_fifo[%d] name[%s] state[%d] set to  MP_FIFO_UNUSED\n",
				i,
				mediaproxy->c_fifo[i]->module_name,
				mediaproxy->has_unusing_fifo);
		}
	}
	return 0;
}

static int mediaproxy_get_consumer_count(void)
{
	return mediaproxy->has_consumer;
}

int media_proxy_produce_init(void **handle, char *module_name, u32 msg_type)
{
	int ret = 0, c_ret = 0;
	struct mediaproxy_session *session;
	pr_err("mediaproxy produce init [%s]\n", module_name);
	ret = session_creat(handle, module_name);
	if (*handle) {
		session = *handle;
		pr_err("mediaproxy produce start connect\n");
		c_ret = mediaproxy_connect(MP_ROLE_PRODUCER, KFIFO_MAX_SIZE, session);
		//connect error,free session
		if (c_ret != 0) {
			pr_err("mediaproxy produce creat and connect error\n");
			kfree(*handle);
			*handle = NULL;
			ret = -1;
		} else {
			pr_err("mediaproxy produce connect [%d]\n", session->fifo_idx);
			session->subscribe_msg_type = msg_type;
			if (session->fifo_idx >= 0) {
				//had connect buf not subscribe or need update msg type
				mediaproxy->p_fifo[session->fifo_idx]->subscribe_msg_type =
					msg_type;
			}
			pr_info("mediaproxy produce msg type :0x%x  module_name:%s\n",
			msg_type, module_name);
		}
	} else {
		pr_err("mediaproxy handle creat error\n");
		return -1;
	}
	return ret;
}
EXPORT_SYMBOL(media_proxy_produce_init);

int notify_msg_to_mediaproxy(void *handle, int num, void *data)
{
	struct mediaproxy_session *session;
	int i = 0;
	struct aml_video_user_data *ptr;

	if (!handle) {
		pr_err("notify mediaproxy handle is null\n");
		return 0;
	}
	if (!data || num <= 0) {
		pr_err("mediaproxy send data is null num:%d\n", num);
		return 0;
	}
	session = handle;
	ptr = data;
	for (i = 0; i < num; i++) {
		if (ptr->message_type & mediaproxy->subscribe_msg_type)
			session_kernel_write(session, 1, ptr);
		else
			pr_dbg("mediaproxy not send data:%d 0x[%x]0x[%x]&[%d]\n", i,
				ptr->message_type,
				mediaproxy->subscribe_msg_type,
				ptr->message_type & mediaproxy->subscribe_msg_type);
		ptr++;
	}
	return num;
}
EXPORT_SYMBOL(notify_msg_to_mediaproxy);

int media_proxy_produce_deinit(void *handle)
{
	struct mediaproxy_session *session;

	if (!handle)
		return 0;

	session = handle;
	pr_info("deinit session\n");
	if (session->session_id >= 0) {
		//not disconnect, get lock
		struct mutex *lock = session->lock;

		mutex_lock(lock);
		pr_info("deinit session %s-%d\n",
			MP_ROLE_STRING(session->role), session->session_id);
		session->session_entry[session->session_id] = NULL;
		if (session->role == MP_ROLE_CONSUMER)
			mediaproxy->has_consumer--;
		if (session->role == MP_ROLE_CONSUMER) {
			mediaproxy->c_fifo[session->fifo_idx]->state = MP_FIFO_UNUSING;
			mediaproxy->c_fifo[session->fifo_idx]->subscribe_msg_type = 0x0;
			update_subscribe_msg_type();
		} else {
			mediaproxy->p_fifo[session->fifo_idx]->state = MP_FIFO_UNUSING;
			mediaproxy->p_fifo[session->fifo_idx]->subscribe_msg_type = 0x0;
			kfifo_free(&session->msg_kfifo);
		}
		mediaproxy->has_unusing_fifo++;
		pr_info("deinit has_unusing_fifo [%d]\n", mediaproxy->has_unusing_fifo);
		kfree(session);
		mutex_unlock(lock);
		wake_up_interruptible(&mediaproxy->transfer_queue);
	} else {
		//disconnect only need free filo and session
		kfree(session);
	}
	return 0;
}
EXPORT_SYMBOL(media_proxy_produce_deinit);

int media_proxy_consumer_init(void **handle, char *module_name, u32 msg_type)
{
	struct mediaproxy_session *session;
	int c_ret = 0;
	int ret = 0;

	pr_info("mediaproxy consumer init [%s]\n", module_name);
	ret = session_creat(handle, module_name);

	if (*handle) {
		session = *handle;
		pr_info("mediaproxy consumer init [%s] connect\n", module_name);

		c_ret = mediaproxy_connect(MP_ROLE_CONSUMER, KFIFO_MAX_SIZE, session);
		//connect error,free session
		if (c_ret != 0) {
			pr_err("mediaproxy consumer creat and connect error\n");
			kfree(*handle);
			*handle = NULL;
			ret = -1;
		} else {
			session->subscribe_msg_type = msg_type;
			update_subscribe_msg_type();
			if (session->fifo_idx >= 0) {
				//had connect buf not subscribe or need update msg type
				mediaproxy->c_fifo[session->fifo_idx]->subscribe_msg_type =
					msg_type;
			}
			pr_info("mediaproxy consumer msg type :0x%x module_name:%s\n",
				msg_type, module_name);
		}
	} else {
		pr_err("mediaproxy handle creat error\n");
		return -1;
	}
	return ret;
}
EXPORT_SYMBOL(media_proxy_consumer_init);

int receive_msg_from_mediaproxy(void *handle,  int num, void *data)
{
	int len = 0;
	int data_copied = 0;
	struct mediaproxy_session *session;

	if (!handle) {
		pr_err("mediaproxy handle is null get msg from proxy\n");
		return 0;
	}
	if (!data) {
		pr_err("mediaproxy data is null get msg from proxy\n");
		return 0;
	}
	session = handle;
	if (session->fifo_idx < 0 || session->fifo_idx >= MAX_SESSION) {
		pr_err("fifo is not connect get msg from proxy\n");
		return 0;
	}
	len =  kfifo_len(&mediaproxy->c_fifo[session->fifo_idx]->msg_kfifo);
	if (len > num)
		len = num;
	if (len > 0)
		data_copied = kfifo_out(&mediaproxy->c_fifo[session->fifo_idx]->msg_kfifo,
						data, len);
	else
		pr_dbg("Mediaproxy c[%d] donot have msg\n", session->fifo_idx);
	return data_copied;
}
EXPORT_SYMBOL(receive_msg_from_mediaproxy);

int media_proxy_consumer_deinit(void *handle)
{
	return media_proxy_produce_deinit(handle);
}
EXPORT_SYMBOL(media_proxy_consumer_deinit);

const struct file_operations mediaproxy_fops = {
	.owner = THIS_MODULE,
	.read = mediaproxy_read,
	.write = mediaproxy_write,
	.open = mediaproxy_open,
	.release = mediaproxy_release,
	.unlocked_ioctl = mediaproxy_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mediaproxy_ioctl,
#endif
};

static ssize_t send_event_thread_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	int ret = 0;
	int rval = 0;

	if (vdec_thread)
		rval = 1;
	else
		rval = 0;

	ret = sprintf(buf, "%d\n", rval);
	return ret;
}

static ssize_t send_event_thread_store(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t size)
{
	int i, tmp = 0;

	i = kstrtoint(buf, -1, &tmp);
	pr_info("send_event_thread thread [%d]\n", tmp);

	if (tmp == 1) {
		//start thread
		if (!k_dec_producer_session) {
			media_proxy_produce_init((void **)&k_dec_producer_session, "decoder",
				MEDIA_VIDEO_STATISTIC_INFO |
				MEDIA_VIDEO_ERROR_EVENT |
				MEDIA_VIDEO_OUTPUT_INFO);
		}
		if (!k_di_producer_session) {
			media_proxy_produce_init((void **)&k_di_producer_session, "di",
				MEDIA_VIDEO_ERROR_EVENT |
				MEDIA_VIDEO_OUTPUT_INFO);
		}
		if (!k_dec_cus_session) {
			media_proxy_consumer_init((void **)&k_dec_cus_session, "dec-consumer",
			MEDIA_VIDEO_STATISTIC_INFO |
					MEDIA_VIDEO_OUTPUT_INFO);
		}
		vdec_thread = kthread_create(producer_thread_fn, NULL, "mediaproxy_produce_thread");
		if (IS_ERR(vdec_thread))
			pr_err("Failed to create vdec_thread thread\n");

		wake_up_process(vdec_thread);
		di_thread = kthread_create(consumer_thread_fn, NULL, "mediaproxy_consumer_thread");
		if (IS_ERR(di_thread))
			pr_err("Failed to create di_thread thread\n");

		wake_up_process(di_thread);
	} else {
		//stop thread
		pr_info("kthread_stop thread\n");
		if (vdec_thread) {
			pr_info("kthread_stop vdec thread\n");
			kthread_stop(vdec_thread);
			vdec_thread = NULL;
		}
		if (di_thread) {
			pr_info("kthread_stop di thread\n");
			kthread_stop(di_thread);
			di_thread = NULL;
		}
		if (k_dec_producer_session)
			media_proxy_produce_deinit(k_dec_producer_session);
		if (k_di_producer_session)
			media_proxy_produce_deinit(k_di_producer_session);
		if (k_dec_cus_session)
			media_proxy_consumer_deinit(k_dec_cus_session);
	}
	return size;
}

static CLASS_ATTR_RW(send_event_thread);

static struct attribute *aml_mediaproxy_class_attrs[] = {
	&class_attr_send_event_thread.attr,
	NULL,
};

ATTRIBUTE_GROUPS(aml_mediaproxy_class);

static struct class aml_mediaproxy_class = {
	.name = "mediaproxy",
	.owner = THIS_MODULE,
	.class_groups = aml_mediaproxy_class_groups,
};

int aml_regist_mediaproxy_class(void)
{
	if (class_register(&aml_mediaproxy_class) < 0)
		pr_err("register class error\n");
	return 0;
}

int aml_unregist_mediaproxy_class(void)
{
	class_unregister(&aml_mediaproxy_class);
	return 0;
}

int __init proxy_init(void)
{
	int result;

	mediaproxy_init();

	mediaproxy_thread = kthread_create(mediaproxy_thread_fn, NULL, "mediaproxy_thread");
	if (IS_ERR(mediaproxy_thread)) {
		pr_err("Failed to create mediaproxy thread\n");
		kfree(mediaproxy);
		return PTR_ERR(mediaproxy_thread);
	}
	wake_up_process(mediaproxy_thread);

	result = register_chrdev(0, DEV_NAME, &mediaproxy_fops);
	if (result < 0) {
		pr_err("Failed to register mediaproxy device\n");
		mediaproxy_cleanup();
		return result;
	}
	mediaproxy->major = result;

	aml_regist_mediaproxy_class();

	mediaproxy_device = device_create(&aml_mediaproxy_class, NULL,
		MKDEV(mediaproxy->major, 0), NULL, DEV_NAME);
	if (IS_ERR(mediaproxy_device)) {
		pr_err("Failed to create mediaproxy device\n");
		unregister_chrdev(mediaproxy->major, DEV_NAME);
		mediaproxy_cleanup();
		return PTR_ERR(mediaproxy_device);
	}
	pr_info("Mediaproxy device loaded with major number %d\n", mediaproxy->major);
	return 0;
}

void __exit proxy_exit(void)
{
	aml_unregist_mediaproxy_class();
	device_destroy(&aml_mediaproxy_class, MKDEV(mediaproxy->major, 0));
	unregister_chrdev(mediaproxy->major, DEV_NAME);
	if (mediaproxy_thread) {
		kthread_stop(mediaproxy_thread);
		pr_info("Mediaproxy thread stopped\n");
	}
	mediaproxy_cleanup();
	pr_info("Mediaproxy device unloaded\n");
}

MODULE_LICENSE("GPL");

