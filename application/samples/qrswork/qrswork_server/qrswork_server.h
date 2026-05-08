/**
 * qrswork server header file
 * Sends heart rate data via SLE connection
 */

#ifndef QRSWORK_SERVER_H
#define QRSWORK_SERVER_H

#include <stdint.h>
#include "sle_ssap_server.h"
#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* Service UUID */
#define SLE_UUID_HEART_SERVICE        0x3333

/* Property UUID */
#define SLE_UUID_HEART_NTF_REPORT     0x3434

/* Property Property */
#define SLE_UUID_HEART_PROPERTIES  (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE)

/* Operation indication */
#define SLE_UUID_HEART_OPERATION_INDICATION  (SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE)

/* Descriptor Property */
#define SLE_UUID_HEART_DESCRIPTOR   (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE)

errcode_t qrswork_server_init(ssaps_read_request_callback ssaps_read_callback, 
    ssaps_write_request_callback ssaps_write_callback);

errcode_t qrswork_server_send_heart_data(const uint8_t *data, uint16_t len);

uint16_t qrswork_server_is_connected(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* QRSWORK_SERVER_H */
