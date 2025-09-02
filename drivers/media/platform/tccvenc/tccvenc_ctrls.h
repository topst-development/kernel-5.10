#ifndef _TCCVENC_CTRLS_H_
#define _TCCVENC_CTRLS_H_

#include <media/v4l2-ctrls.h>

#include "tccvenc.h"
#include "tccvenc_debug.h"

struct tcc_venc_ctx;

int tccvenc_ctrls_init(struct tcc_venc_ctx *ctx);

#endif
