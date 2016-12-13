#include "ua_client_highlevel_extra.h"
#include "ua_client_internal.h"
#include "ua_util.h"
#include "ua_types_generated_encoding_binary.h"

#ifdef UA_ENABLE_SUBSCRIPTIONS /* conditional compilation */

UA_StatusCode UA_Client_Subscriptions_new2(UA_Client *client, UA_SubscriptionSettings settings,
                                          UA_UInt32 *newSubscriptionId, SubcriptionHandler hander, void* context) {
    UA_CreateSubscriptionRequest request;
    UA_CreateSubscriptionRequest_init(&request);
    request.requestedPublishingInterval = settings.requestedPublishingInterval;
    request.requestedLifetimeCount = settings.requestedLifetimeCount;
    request.requestedMaxKeepAliveCount = settings.requestedMaxKeepAliveCount;
    request.maxNotificationsPerPublish = settings.maxNotificationsPerPublish;
    request.publishingEnabled = settings.publishingEnabled;
    request.priority = settings.priority;

    UA_CreateSubscriptionResponse response = UA_Client_Service_createSubscription(client, request);
    UA_StatusCode retval = response.responseHeader.serviceResult;
    if(retval != UA_STATUSCODE_GOOD)
        goto cleanup;

    UA_Client_Subscription *newSub = UA_malloc(sizeof(UA_Client_Subscription));
    if(!newSub) {
        retval = UA_STATUSCODE_BADOUTOFMEMORY;
        goto cleanup;
    }

    LIST_INIT(&newSub->MonitoredItems);
    newSub->LifeTime = response.revisedLifetimeCount;
    newSub->KeepAliveCount = response.revisedMaxKeepAliveCount;
    newSub->PublishingInterval = response.revisedPublishingInterval;
    newSub->SubscriptionID = response.subscriptionId;
    newSub->NotificationsPerPublish = request.maxNotificationsPerPublish;
    newSub->Priority = request.priority;
    newSub->subcriptionHandler = hander;
    newSub->subscriptionHandlerContext = context;
    LIST_INSERT_HEAD(&client->subscriptions, newSub, listEntry);

    if(newSubscriptionId)
        *newSubscriptionId = newSub->SubscriptionID;

 cleanup:
    UA_CreateSubscriptionResponse_deleteMembers(&response);
    return retval;
}

UA_StatusCode UA_Client_Subscriptions_GetSetting(UA_Client *client, UA_UInt32 subscriptionId,
                                                 UA_SubscriptionSettings *setting_out)
{
    if (setting_out == NULL) {
        return UA_STATUSCODE_BADSYNTAXERROR;
    }
    UA_Client_Subscription *sub = NULL;
    LIST_FOREACH(sub, &client->subscriptions, listEntry) {
        if(sub->SubscriptionID == subscriptionId) {
            setting_out->requestedPublishingInterval = sub->PublishingInterval;
            setting_out->requestedLifetimeCount = sub->LifeTime;
            setting_out->requestedMaxKeepAliveCount = sub->KeepAliveCount;
            setting_out->maxNotificationsPerPublish = sub->NotificationsPerPublish;
            setting_out->priority = sub->Priority;
            return UA_STATUSCODE_GOOD;
        }
    }
    return UA_STATUSCODE_BADSUBSCRIPTIONIDINVALID;
}

UA_StatusCode UA_Client_Subscriptions_modify(UA_Client *client, UA_UInt32 subscriptionId,
                                          UA_SubscriptionSettings settings)
{
    UA_Client_Subscription *sub = NULL;
    LIST_FOREACH(sub, &client->subscriptions, listEntry) {
        if(sub->SubscriptionID == subscriptionId) {
            break;
        }
    }
    if (sub == NULL) {
        return UA_STATUSCODE_BADSUBSCRIPTIONIDINVALID;
    }

    UA_ModifySubscriptionRequest request;
    UA_ModifySubscriptionRequest_init(&request);
    request.subscriptionId = subscriptionId;
    request.requestedPublishingInterval = settings.requestedPublishingInterval;
    request.requestedLifetimeCount = settings.requestedLifetimeCount;
    request.requestedMaxKeepAliveCount = settings.requestedMaxKeepAliveCount;
    request.maxNotificationsPerPublish = settings.maxNotificationsPerPublish;
    request.priority = settings.priority;

    UA_ModifySubscriptionResponse response = UA_Client_Service_modifySubscription(client, request);
    UA_StatusCode retval = response.responseHeader.serviceResult;
    if(retval != UA_STATUSCODE_GOOD)
        goto cleanup;

    sub->LifeTime = response.revisedLifetimeCount;
    sub->KeepAliveCount = response.revisedMaxKeepAliveCount;
    sub->PublishingInterval = response.revisedPublishingInterval;
    sub->NotificationsPerPublish = request.maxNotificationsPerPublish;
    sub->Priority = request.priority;

 cleanup:
    UA_ModifySubscriptionResponse_deleteMembers(&response);
    return retval;
}

UA_StatusCode
UA_Client_Subscriptions_addMonitoredItems(UA_Client *client, UA_UInt32 subscriptionId,
                                         UA_NodeId* nodeIds, UA_UInt32 nodeCnt, UA_UInt32 attributeID,
                                         UA_MonitoredItemHandlingFunction handlingFunction,
                                         void *handlingContext, UA_UInt32 *newMonitoredItemIds,
                                          UA_UInt32 *clientHandles, bool assignClientHandles)
{
    UA_UInt32 i;
    UA_Client_Subscription *sub;
    LIST_FOREACH(sub, &client->subscriptions, listEntry) {
        if(sub->SubscriptionID == subscriptionId)
            break;
    }
    if(!sub)
        return UA_STATUSCODE_BADSUBSCRIPTIONIDINVALID;

    /* Send the request */
    UA_CreateMonitoredItemsRequest request;
    UA_CreateMonitoredItemsRequest_init(&request);
    request.subscriptionId = subscriptionId;

    UA_MonitoredItemCreateRequest* items =
    (UA_MonitoredItemCreateRequest*)UA_Array_new(nodeCnt, &UA_TYPES[UA_TYPES_MONITOREDITEMCREATEREQUEST]);
    memset(items, 0, nodeCnt * sizeof(UA_MonitoredItemCreateRequest));
    for (i = 0; i < nodeCnt; i++ ) {
        UA_copy(&nodeIds[i], &items[i].itemToMonitor.nodeId, &UA_TYPES[UA_TYPES_NODEID]);
        items[i].itemToMonitor.attributeId = attributeID;
        items[i].monitoringMode = UA_MONITORINGMODE_REPORTING;
        if (assignClientHandles) {
            items[i].requestedParameters.clientHandle = clientHandles[i];
            client->monitoredItemHandles = client->monitoredItemHandles > clientHandles[i] ?
                        client->monitoredItemHandles : clientHandles[i];
        } else {
            items[i].requestedParameters.clientHandle = ++(client->monitoredItemHandles);
        }
        items[i].requestedParameters.samplingInterval = sub->PublishingInterval;
        items[i].requestedParameters.discardOldest = true;
        items[i].requestedParameters.queueSize = 1;
    }
    request.itemsToCreate = items;
    request.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH;
    request.itemsToCreateSize = nodeCnt;
    UA_CreateMonitoredItemsResponse response = UA_Client_Service_createMonitoredItems(client, request);

    // slight misuse of retval here to check if the deletion was successfull.
    UA_StatusCode retval;
    if(response.resultsSize == 0)
        retval = response.responseHeader.serviceResult;
    else
        retval = response.results[0].statusCode;
    if(retval != UA_STATUSCODE_GOOD) {
        UA_CreateMonitoredItemsResponse_deleteMembers(&response);
        return retval;
    }
    if (response.resultsSize != nodeCnt) {
        retval = response.responseHeader.serviceResult;
        UA_LOG_ERROR(client->config.logger, UA_LOGCATEGORY_CLIENT,
                      "Created a monitored items failed 0x%0x inputSize %d outPutSize %d",
                     retval, nodeCnt, response.resultsSize);
        return retval;
    }
    memset(newMonitoredItemIds, 0, nodeCnt * sizeof(UA_UInt32));
    /* Create the handler */
    for (i = 0; i < nodeCnt; i++) {
        UA_Client_MonitoredItem *newMon = UA_malloc(sizeof(UA_Client_MonitoredItem));
        newMon->MonitoringMode = UA_MONITORINGMODE_REPORTING;
        UA_NodeId_copy(&nodeIds[i], &newMon->monitoredNodeId);
        newMon->AttributeID = attributeID;
        newMon->ClientHandle = items[i].requestedParameters.clientHandle;
        newMon->SamplingInterval = sub->PublishingInterval;
        newMon->QueueSize = 1;
        newMon->DiscardOldest = true;
        newMon->handler = handlingFunction;
        newMon->handlerContext = handlingContext;
        newMon->MonitoredItemId = response.results[i].monitoredItemId;
        LIST_INSERT_HEAD(&sub->MonitoredItems, newMon, listEntry);
        newMonitoredItemIds[i] = newMon->MonitoredItemId;
        clientHandles[i] = newMon->ClientHandle;
    }

    UA_LOG_DEBUG(client->config.logger, UA_LOGCATEGORY_CLIENT,
                 "Created a monitored item with client handle %u", client->monitoredItemHandles);

    UA_CreateMonitoredItemsResponse_deleteMembers(&response);
    UA_Array_delete(items, nodeCnt, &UA_TYPES[UA_TYPES_MONITOREDITEMCREATEREQUEST]);
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode UA_EXPORT
UA_Client_Subscriptions_removeMonitoredItems(UA_Client *client,
                                            UA_UInt32 subscriptionId,
                                            UA_UInt32* monitoredItemIds, UA_UInt32 nodeCnt)
{
    UA_Client_Subscription *sub;
    LIST_FOREACH(sub, &client->subscriptions, listEntry) {
        if(sub->SubscriptionID == subscriptionId)
            break;
    }
    if(!sub)
        return UA_STATUSCODE_BADSUBSCRIPTIONIDINVALID;

    UA_Client_MonitoredItem *mon;
    UA_UInt32 newCnt = 0;
    for (UA_UInt32 i = 0; i < nodeCnt; i++) {
        LIST_FOREACH(mon, &sub->MonitoredItems, listEntry) {
            if(mon->MonitoredItemId == monitoredItemIds[i]) {
                monitoredItemIds[newCnt++] = monitoredItemIds[i];
                break;
            }
        }
    }

    /* remove the monitoreditem remotely */
    UA_DeleteMonitoredItemsRequest request;
    UA_DeleteMonitoredItemsRequest_init(&request);
    request.subscriptionId = sub->SubscriptionID;
    request.monitoredItemIdsSize = newCnt;
    request.monitoredItemIds = monitoredItemIds;
    UA_DeleteMonitoredItemsResponse response = UA_Client_Service_deleteMonitoredItems(client, request);
    UA_Client_MonitoredItem *tempMon;
    UA_StatusCode retval = response.responseHeader.serviceResult;
    if (retval == UA_STATUSCODE_GOOD && response.resultsSize == newCnt) {
        for (UA_UInt32 i = 0; i < newCnt; i++) {
            LIST_FOREACH_SAFE(mon, &sub->MonitoredItems, listEntry, tempMon) {
                if (mon->MonitoredItemId == monitoredItemIds[i]) {
                    LIST_REMOVE(mon, listEntry);
                    UA_NodeId_deleteMembers(&mon->monitoredNodeId);
                    UA_free(mon);
                    break;
                }
            }
        }
    }
    UA_DeleteMonitoredItemsResponse_deleteMembers(&response);
    if(retval != UA_STATUSCODE_GOOD && retval != UA_STATUSCODE_BADMONITOREDITEMIDINVALID) {
        UA_LOG_INFO(client->config.logger, UA_LOGCATEGORY_CLIENT,
                    "Could not remove %u monitoreditems %u with statuscode 0x%08x",
                    newCnt, retval);
        return retval;
    }

    return UA_STATUSCODE_GOOD;
}

bool isHaveSubscription(UA_Client* client)
{
    return client && (!(LIST_EMPTY(&client->subscriptions)));
}

bool clearSubscription(UA_Client* client)
{
    int count = 0;
    UA_Client_Subscription *sub;
    LIST_FOREACH(sub, &client->subscriptions, listEntry) {
        count++;
    }
    if (count <= 0) {
        return true;
    }
    int i = 0;
    UA_UInt32* subScriptions = (UA_UInt32*)UA_Array_new(count, &UA_TYPES[UA_TYPES_UINT32]);
    LIST_FOREACH(sub, &client->subscriptions, listEntry) {
        subScriptions[i++] = sub->SubscriptionID;
    }
    for (int i = 0; i < count; i++) {
        UA_Client_Subscriptions_remove(client, subScriptions[i]);
    }
    UA_Array_delete(subScriptions, count, &UA_TYPES[UA_TYPES_UINT32]);
    return true;
}

#endif /* UA_ENABLE_SUBSCRIPTIONS */
