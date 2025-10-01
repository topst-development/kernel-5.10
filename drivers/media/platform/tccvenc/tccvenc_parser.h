#ifndef TCCVENC_PARSER_H
#define TCCVENC_PARSER_H

#include <linux/types.h>

/*
 * Strip VUI from all SPS NAL units inside an Annex-B H.264 buffer, in-place.
 *  - buf:  Annex-B formatted byte stream (start codes present).
 *  - len_inout: in:  buffer length, out: new length after rewrite.
 * Returns 0 on success, negative errno on allocation failure.
 */
int tccvenc_h264_strip_vui_in_place_annexb(u8 *buf, int *len_inout);

#endif /* TCCVENC_PARSER_H */
