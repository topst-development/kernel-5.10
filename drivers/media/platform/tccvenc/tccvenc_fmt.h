#ifndef _TCCVENC_FMT_H_
#define _TCCVENC_FMT_H_

#include <linux/videodev2.h>
#include <media/v4l2-ioctl.h>

#include "tccvenc.h"
#include "tccvenc_debug.h"

#define MAX_WIDTH  3840
#define MAX_HEIGHT 2160

#define DEFAULT_FRAMERATE_NUM 1001
#define DEFAULT_FRAMERATE_DENOM 30000

struct tccvenc_fmt {
	u32 fourcc;
	const char *desc;
};

int tccvenc_enum_fmt(struct file *file, void *priv,
                     struct v4l2_fmtdesc *f);

int tccvenc_try_fmt(struct file *file, void *priv,
                    struct v4l2_format *f);

int tccvenc_s_fmt(struct file *file, void *priv,
                  struct v4l2_format *f);

int tccvenc_g_fmt(struct file *file, void *priv, 
                  struct v4l2_format *f);

int tccvenc_s_parm(struct file *file, void *fh,
                  struct v4l2_streamparm *a);

int tccvenc_g_parm(struct file *file, void *priv,
			      struct v4l2_streamparm *a);
#endif