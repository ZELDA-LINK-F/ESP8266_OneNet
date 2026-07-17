#ifndef __AHT20_SOFT_H__
#define __AHT20_SOFT_H__

#include <stdint.h>

uint8_t AHT20_SoftInit(void);
uint8_t AHT20_SoftRead(float *t, float *h);

#endif /* __AHT20_SOFT_H__ */
