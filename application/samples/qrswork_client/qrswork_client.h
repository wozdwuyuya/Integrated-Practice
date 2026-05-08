/**
 * qrswork client header file
 * Receives heart rate data via SLE connection
 */

#ifndef QRSWORK_CLIENT_H
#define QRSWORK_CLIENT_H

#include <stdint.h>
#include "sle_ssap_client.h"
#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifndef QRSWORK_SERVER_NAME
#define QRSWORK_SERVER_NAME "qrswork_heart_server"
#endif

errcode_t qrswork_client_init(ssapc_notification_callback notification_cb, 
                               ssapc_indication_callback indication_cb);

void qrswork_notification_cb(uint8_t client_id, uint16_t conn_id, 
                             ssapc_handle_value_t *data, errcode_t status);

void qrswork_indication_cb(uint8_t client_id, uint16_t conn_id, 
                           ssapc_handle_value_t *data, errcode_t status);

uint16_t get_qrswork_conn_id(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* QRSWORK_CLIENT_H */
