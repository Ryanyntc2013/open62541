#ifndef UA_CLIENT_HIGHLEVEL_EXTRA_H_
#define UA_CLIENT_HIGHLEVEL_EXTRA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ua_client_highlevel.h"

typedef void (*SubcriptionHandler)(UA_UInt32 subId, UA_DataChangeNotification  *dataNotify, void *context);

UA_StatusCode  UA_EXPORT
UA_Client_Subscriptions_GetSetting(UA_Client *client, UA_UInt32 subscriptionId, UA_SubscriptionSettings* setting_out);


UA_StatusCode UA_EXPORT
UA_Client_Subscriptions_new2(UA_Client *client, UA_SubscriptionSettings settings,
                            UA_UInt32 *newSubscriptionId, SubcriptionHandler hander, void* context);

UA_StatusCode UA_EXPORT
UA_Client_Subscriptions_modify(UA_Client *client, UA_UInt32 subscriptionId, UA_SubscriptionSettings settings);


UA_StatusCode UA_EXPORT
UA_Client_Subscriptions_addMonitoredItems(UA_Client *client, UA_UInt32 subscriptionId,
                                         UA_NodeId* nodeIds, UA_UInt32 nodeCnt, UA_UInt32 attributeID,
                                         UA_MonitoredItemHandlingFunction handlingFunction,
                                         void *handlingContext, UA_UInt32 *newMonitoredItemIds,
                                          UA_UInt32 *clientHandles, bool assignClientHandles);

UA_StatusCode UA_EXPORT
UA_Client_Subscriptions_removeMonitoredItems(UA_Client *client,
                                            UA_UInt32 subscriptionId,
                                            UA_UInt32* monitoredItemIds, UA_UInt32 nodeCnt);

UA_EXPORT bool isHaveSubscription(UA_Client* client);
UA_EXPORT bool clearSubscription(UA_Client* client);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* UA_CLIENT_HIGHLEVEL_EXTRA_H_ */
