/**
 * qrswork northbound data uplink interface.
 */

#ifndef QRSWORK_NORTHBOUND_CLIENT_H
#define QRSWORK_NORTHBOUND_CLIENT_H

#include <stdint.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

int northbound_client_start(void);
void northbound_client_publish_heart(uint32_t adc, uint32_t bpm);
void northbound_client_publish_raw(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* QRSWORK_NORTHBOUND_CLIENT_H */
