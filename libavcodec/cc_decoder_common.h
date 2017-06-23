
#ifndef CC_DECODER_COMMON_H
#define CC_DECODER_COMMON_H

#include "cc_decoder_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

cc_decode* init_cc_decode(void);
void dinit_cc_decode(cc_decode **ctx);
void flush_cc_decode(cc_decode *ctx);


#ifdef __cplusplus
}
#endif

#endif /* CC_DECODER_COMMON_H */
