/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/
#include "opener_api.h"
#include "cipcommon.h"
#include "endianconv.h"
#include "ciperror.h"
#include "trace.h"
#include "enipmessage.h"

#include "cipmessagerouter.h"

CipMessageRouterRequest g_message_router_request;

/** @brief A class registry list node
 *
 * A linked list of this  object is the registry of classes known to the message router
 * for small devices with very limited memory it could make sense to change this list into an
 * array with a given max size for removing the need for having to dynamically allocate
 * memory. The size of the array could be a parameter in the platform config file.
 */
typedef struct cip_message_router_object {
  struct cip_message_router_object *next; /**< link */
  CipClass *cip_class; /**< object */
} CipMessageRouterObject;

/** @brief Pointer to first registered object in MessageRouter*/
CipMessageRouterObject *g_first_object = NULL;

/** @brief Message Router instance #1 data structure (vendor-specific attributes) */
typedef struct {
  CipUint supported_objects_number;        /* Number of supported classes */
  CipUint *supported_objects_class_ids;    /* Array of class IDs */
  CipUint max_connections_supported;        /* Maximum connections supported */
  CipUint number_of_current_connections;   /* Current number of active connections */
  CipUint *active_connections;             /* Array of active connection IDs */
  CipUint active_connections_count;        /* Number of active connections */
} CipMessageRouterInstanceData;

/** @brief Register a CIP Class to the message router
 *  @param cip_class Pointer to a class object to be registered.
 *  @return kEipStatusOk on success
 *          kEipStatusError on no memory available to register more objects
 */
EipStatus RegisterCipClass(CipClass *cip_class);

/** @brief Create Message Router Request structure out of the received data.
 *
 * Parses the UCMM header consisting of: service, IOI size, IOI, data into a request structure
 * @param data pointer to the message data received
 * @param data_length number of bytes in the message
 * @param message_router_request pointer to structure of MRRequest data item.
 * @return kEipStatusOk on success. otherwise kEipStatusError
 */
CipError CreateMessageRouterRequestStructure(const EipUint8 *data,
                                             EipInt16 data_length,
                                             CipMessageRouterRequest *message_router_request);

void InitializeCipMessageRouterClass(CipClass *cip_class) {

  CipClass *meta_class = cip_class->class_instance.cip_class;

  InsertAttribute( (CipInstance *) cip_class, 1, kCipUint, EncodeCipUint, NULL,
                   (void *) &cip_class->revision, kGetableSingleAndAll );                                                          /* revision */
  InsertAttribute( (CipInstance *) cip_class, 2, kCipUint, EncodeCipUint, NULL,
                   (void *) &cip_class->number_of_instances, kGetableSingle );                                                          /*  largest instance number */
  InsertAttribute( (CipInstance *) cip_class, 3, kCipUint, EncodeCipUint, NULL,
                   (void *) &cip_class->number_of_instances, kGetableSingle );                                                          /* number of instances currently existing*/
  InsertAttribute( (CipInstance *) cip_class, 4, kCipUint, EncodeCipUint, NULL,
                   (void *) &kCipUintZero, kGetableAll );                                                          /* optional attribute list - default = 0 */
  InsertAttribute( (CipInstance *) cip_class, 5, kCipUint, EncodeCipUint, NULL,
                   (void *) &kCipUintZero, kGetableAll );                                                          /* optional service list - default = 0 */
  InsertAttribute( (CipInstance *) cip_class, 6, kCipUint, EncodeCipUint, NULL,
                   (void *) &meta_class->highest_attribute_number,
                   kGetableSingleAndAll );                                                                                                          /* max class attribute number*/
  InsertAttribute( (CipInstance *) cip_class, 7, kCipUint, EncodeCipUint, NULL,
                   (void *) &cip_class->highest_attribute_number,
                   kGetableSingleAndAll );                                                                                                         /* max instance attribute number*/

  InsertService(meta_class,
                kGetAttributeAll,
                &GetAttributeAll,
                "GetAttributeAll");                                                 /* bind instance services to the metaclass*/
  InsertService(meta_class,
                kGetAttributeSingle,
                &GetAttributeSingle,
                "GetAttributeSingle");
}

/** @brief Encode Message Router instance #1 SupportedObjects attribute
 *  This is a vendor-specific STRUCT attribute matching Rockwell's implementation
 */
void EncodeMessageRouterSupportedObjects(const void *const data,
                                         ENIPMessage *const outgoing_message) {
  CipMessageRouterInstanceData *instance_data = (CipMessageRouterInstanceData *)data;
  
  /* Count registered classes dynamically */
  CipUint class_count = 0;
  CipMessageRouterObject *obj = g_first_object;
  CipBool file_object_found = false;
  while(obj != NULL) {
    if(obj->cip_class->class_code == 0x37) {
      file_object_found = true;
    }
    class_count++;
    obj = obj->next;
  }
  
  // Debug logging removed - use OPENER_TRACE_INFO if needed for debugging
  // OPENER_TRACE_INFO("Message Router: Encoding SupportedObjects - class_count=%u, File Object (0x37) %s\n",
  //                  class_count, file_object_found ? "FOUND" : "NOT FOUND");
  
  /* Always update the array to ensure it's current (classes may be registered after initialization) */
  if(instance_data->supported_objects_class_ids != NULL && 
     instance_data->supported_objects_number == class_count) {
    /* Array exists and size matches - just refresh the data */
    obj = g_first_object;
    CipUint idx = 0;
    while(obj != NULL && idx < class_count) {
      instance_data->supported_objects_class_ids[idx] = obj->cip_class->class_code;
      obj = obj->next;
      idx++;
    }
  } else {
    /* Need to reallocate array */
    if(instance_data->supported_objects_class_ids != NULL) {
      CipFree(instance_data->supported_objects_class_ids);
    }
    instance_data->supported_objects_class_ids = (CipUint *)CipCalloc(class_count, sizeof(CipUint));
    if(instance_data->supported_objects_class_ids != NULL) {
      obj = g_first_object;
      CipUint idx = 0;
      while(obj != NULL && idx < class_count) {
        instance_data->supported_objects_class_ids[idx] = obj->cip_class->class_code;
        // Debug logging removed - use OPENER_TRACE_INFO if needed for debugging
        // OPENER_TRACE_INFO("Message Router: Adding class ID %lu (0x%02lX) to array at index %u\n",
        //                  (unsigned long)obj->cip_class->class_code, (unsigned long)obj->cip_class->class_code, idx);
        obj = obj->next;
        idx++;
      }
      instance_data->supported_objects_number = class_count;
    } else {
      class_count = 0; /* Fallback if allocation fails */
      instance_data->supported_objects_number = 0;
      OPENER_TRACE_ERR("Message Router: ERROR - Failed to allocate ClassesId array!\n");
    }
  }
  
  /* Save starting position to calculate STRUCT size */
  size_t struct_start_length = outgoing_message->used_message_length;
  
  /* Encode STRUCT: ClassesId array (UINT array)
   * Note: The STRUCT format is: ArrayLen, Array elements, MaxConnectionsSupported, NumberOfCurrentConnections
   * The "Number" field is not encoded separately - it's just the array length */
  if(instance_data->supported_objects_class_ids != NULL) {
    // Debug logging removed - use OPENER_TRACE_INFO if needed for debugging
    // OPENER_TRACE_INFO("Message Router: Encoding ClassesId array - length=%u, classes: ", instance_data->supported_objects_number);
    AddIntToMessage(instance_data->supported_objects_number, outgoing_message); /* Array length */
    for(CipUint i = 0; i < instance_data->supported_objects_number; i++) {
      // OPENER_TRACE_INFO("%u ", instance_data->supported_objects_class_ids[i]);
      AddIntToMessage(instance_data->supported_objects_class_ids[i], outgoing_message);
    }
    // OPENER_TRACE_INFO("(encoded %u elements)\n", instance_data->supported_objects_number);
  } else {
    AddIntToMessage(0, outgoing_message); /* Empty array */
    OPENER_TRACE_ERR("Message Router: ERROR - ClassesId array is NULL!\n");  // Keep this as ERROR
  }
  
  /* Encode STRUCT: MaxConnectionsSupported (UINT) */
  AddIntToMessage(instance_data->max_connections_supported, outgoing_message);
  
  /* Encode STRUCT: NumberOfCurrentConnections (UINT) */
  AddIntToMessage(instance_data->number_of_current_connections, outgoing_message);
  
  size_t struct_size = outgoing_message->used_message_length - struct_start_length;
  // Debug logging removed - use OPENER_TRACE_INFO if needed for debugging
  // OPENER_TRACE_INFO("Message Router: SupportedObjects STRUCT encoded - STRUCT size: %u bytes (ArrayLen=%u, ArrayElements=%u, MaxConn=%u, NumConn=%u)\n",
  //                  (unsigned)struct_size,
  //                  instance_data->supported_objects_number,
  //                  instance_data->supported_objects_number,
  //                  instance_data->max_connections_supported,
  //                  instance_data->number_of_current_connections);
}

/** @brief Encode Message Router instance #1 ActiveConnections attribute
 *  This is a vendor-specific array attribute
 */
void EncodeMessageRouterActiveConnections(const void *const data,
                                          ENIPMessage *const outgoing_message) {
  CipMessageRouterInstanceData *instance_data = (CipMessageRouterInstanceData *)data;
  
  /* Encode array: UINT array */
  AddIntToMessage(instance_data->active_connections_count, outgoing_message); /* Array length */
  for(CipUint i = 0; i < instance_data->active_connections_count; i++) {
    AddIntToMessage(instance_data->active_connections[i], outgoing_message);
  }
}

EipStatus CipMessageRouterInit() {

  CipClass *message_router = CreateCipClass(kCipMessageRouterClassCode, /* class code */
                                            7, /* # of class attributes */
                                            7, /* # highest class attribute number */
                                            2, /* # of class services */
                                            2, /* # of instance attributes (vendor-specific) */
                                            2, /* # highest instance attribute number */
                                            2, /* # of instance services (GetAttributeSingle, GetAttributeAll) */
                                            1, /* # of instances */
                                            "message router", /* class name */
                                            1, /* # class revision*/
                                            InitializeCipMessageRouterClass); /* # function pointer for initialization*/
  if(NULL == message_router) {
    return kEipStatusError;
  }
  InsertService(message_router,
                kGetAttributeSingle,
                &GetAttributeSingle,
                "GetAttributeSingle");
  InsertService(message_router,
                kGetAttributeAll,
                &GetAttributeAll,
                "GetAttributeAll");

  /* Initialize instance #1 with vendor-specific attributes */
  CipInstance *instance = GetCipInstance(message_router, 1);
  if(NULL != instance) {
    /* Allocate and initialize instance data structure */
    CipMessageRouterInstanceData *instance_data = (CipMessageRouterInstanceData *)CipCalloc(1, sizeof(CipMessageRouterInstanceData));
    if(NULL == instance_data) {
      OPENER_TRACE_ERR("Message Router: ERROR - Failed to allocate instance data!\n");
      return kEipStatusError;
    }
    
    /* Initialize with 0 classes - will be populated dynamically when queried */
    instance_data->supported_objects_number = 0;
    instance_data->supported_objects_class_ids = NULL;
    instance_data->max_connections_supported = 24; /* Default value, can be configured */
    instance_data->number_of_current_connections = 0; /* Will be updated as connections are made */
    instance_data->active_connections_count = 0;
    instance_data->active_connections = NULL; /* Allocate when needed */
    
    instance->data = (void *)instance_data;
    
    /* Add instance attributes */
    InsertAttribute(instance, 1, kCipAny, EncodeMessageRouterSupportedObjects, NULL,
                    instance_data, kGetableSingleAndAll);
    InsertAttribute(instance, 2, kCipAny, EncodeMessageRouterActiveConnections, NULL,
                    instance_data, kGetableSingleAndAll);
    
  } else {
    OPENER_TRACE_ERR("Message Router: ERROR - Instance #1 not found after creation!\n");
    return kEipStatusError;
  }

  /* reserved for future use -> set to zero */
  return kEipStatusOk;
}

/** @brief Get the registered MessageRouter object corresponding to ClassID.
 *  given a class ID, return a pointer to the registration node for that object
 *
 *  @param class_id Class code to be searched for.
 *  @return Pointer to registered message router object
 *      NULL .. Class not registered
 */
CipMessageRouterObject *GetRegisteredObject(EipUint32 class_id) {
  CipMessageRouterObject *object = g_first_object; /* get pointer to head of class registration list */

  while(NULL != object) /* for each entry in list*/
  {
    OPENER_ASSERT(NULL != object->cip_class);
    if(object->cip_class->class_code == class_id) {
      return object; /* return registration node if it matches class ID*/
    }
    object = object->next;
  }
  return NULL;
}

CipClass *GetCipClass(const CipUdint class_code) {
  CipMessageRouterObject *message_router_object =
    GetRegisteredObject(class_code);

  if(message_router_object) {
    return message_router_object->cip_class;
  } else {
    return NULL;
  }
}

CipInstance *GetCipInstance(const CipClass *RESTRICT const cip_class,
                            const CipInstanceNum instance_number) {

  if(instance_number == 0) {
    return (CipInstance *) cip_class; /* if the instance number is zero, return the class object itself*/

  }
  /* pointer to linked list of instances from the class object*/
  for(CipInstance *instance = cip_class->instances; instance;
      instance = instance->next)                                                         /* follow the list*/
  {
    if(instance->instance_number == instance_number) {
      return instance; /* if the number matches, return the instance*/
    }
  }

  return NULL;
}

EipStatus RegisterCipClass(CipClass *cip_class) {
  CipMessageRouterObject **message_router_object = &g_first_object;

  while(*message_router_object) {
    message_router_object = &(*message_router_object)->next; /* follow the list until p points to an empty link (list end)*/

  }
  *message_router_object =
    (CipMessageRouterObject *) CipCalloc(1, sizeof(CipMessageRouterObject) );                      /* create a new node at the end of the list*/
  if(*message_router_object == 0) {
    return kEipStatusError; /* check for memory error*/

  }
  (*message_router_object)->cip_class = cip_class; /* fill in the new node*/
  (*message_router_object)->next = NULL;

  return kEipStatusOk;
}

EipStatus NotifyMessageRouter(EipUint8 *data,
                              int data_length,
                              CipMessageRouterResponse *message_router_response,
                              const struct sockaddr *const originator_address,
                              const CipSessionHandle encapsulation_session) {
  EipStatus eip_status = kEipStatusOkSend;
  CipError status = kCipErrorSuccess;

  OPENER_TRACE_INFO("NotifyMessageRouter: routing unconnected message\n");
  if(kCipErrorSuccess !=
     (status =
        CreateMessageRouterRequestStructure(data, data_length,
                                            &g_message_router_request) ) ) {                                             /* error from create MR structure*/
    OPENER_TRACE_ERR(
      "NotifyMessageRouter: error from createMRRequeststructure\n");
    message_router_response->general_status = status;
    message_router_response->size_of_additional_status = 0;
    message_router_response->reserved = 0;
    message_router_response->reply_service =
      (0x80 | g_message_router_request.service);
  } else {
    /* forward request to appropriate Object if it is registered*/
    CipMessageRouterObject *registered_object = GetRegisteredObject(
      g_message_router_request.request_path.class_id);
    if(registered_object == 0) {
      OPENER_TRACE_ERR(
        "NotifyMessageRouter: sending CIP_ERROR_OBJECT_DOES_NOT_EXIST reply, class id 0x%x is not registered\n",
        (unsigned ) g_message_router_request.request_path.class_id);
      message_router_response->general_status = kCipErrorPathDestinationUnknown; /*according to the test tool this should be the correct error flag instead of CIP_ERROR_OBJECT_DOES_NOT_EXIST;*/
      message_router_response->size_of_additional_status = 0;
      message_router_response->reserved = 0;
      message_router_response->reply_service =
        (0x80 | g_message_router_request.service);
    } else {
      /* call notify function from Object with ClassID (gMRRequest.RequestPath.ClassID)
         object will or will not make an reply into gMRResponse*/
      message_router_response->reserved = 0;
      OPENER_ASSERT(NULL != registered_object->cip_class); OPENER_TRACE_INFO(
        "NotifyMessageRouter: calling notify function of class '%s'\n",
        registered_object->cip_class->class_name);
      eip_status = NotifyClass(registered_object->cip_class,
                               &g_message_router_request,
                               message_router_response,
                               originator_address,
                               encapsulation_session);

#ifdef OPENER_TRACE_ENABLED
      if (eip_status == kEipStatusError) {
        OPENER_TRACE_ERR(
          "notifyMR: notify function of class '%s' returned an error\n",
          registered_object->cip_class->class_name);
      } else if (eip_status == kEipStatusOk) {
        OPENER_TRACE_INFO(
          "notifyMR: notify function of class '%s' returned no reply\n",
          registered_object->cip_class->class_name);
      } else {
        // OPENER_TRACE_INFO("notifyMR: notify function of class '%s' returned a reply\n",
        //                   registered_object->cip_class->class_name); // Disabled for less noise
      }
#endif
    }
  }
  return eip_status;
}

CipError CreateMessageRouterRequestStructure(const EipUint8 *data,
                                             EipInt16 data_length,
                                             CipMessageRouterRequest *message_router_request)
{

  message_router_request->service = *data;
  data++;
  data_length--;

  size_t number_of_decoded_bytes;
  const EipStatus path_result =
    DecodePaddedEPath(&(message_router_request->request_path),
                      &data,
                      &number_of_decoded_bytes);
  if(path_result != kEipStatusOk) {
    return kCipErrorPathSegmentError;
  }

  if(number_of_decoded_bytes > data_length) {
    return kCipErrorPathSizeInvalid;
  } else {
    message_router_request->data = data;
    message_router_request->request_data_size = data_length -
                                                number_of_decoded_bytes;
    return kCipErrorSuccess;
  }
}

void DeleteAllClasses(void) {
  CipMessageRouterObject *message_router_object = g_first_object; /* get pointer to head of class registration list */
  CipMessageRouterObject *message_router_object_to_delete = NULL;
  CipInstance *instance = NULL;
  CipInstance *instance_to_delete = NULL;

  while(NULL != message_router_object) {
    message_router_object_to_delete = message_router_object;
    message_router_object = message_router_object->next;

    instance = message_router_object_to_delete->cip_class->instances;
    while(NULL != instance) {
      instance_to_delete = instance;
      instance = instance->next;
      if(message_router_object_to_delete->cip_class->number_of_attributes) /* if the class has instance attributes */
      { /* then free storage for the attribute array */
        CipFree(instance_to_delete->attributes);
      }
      CipFree(instance_to_delete);
    }

    /* free meta class data*/
    CipClass *meta_class =
      message_router_object_to_delete->cip_class->class_instance.cip_class;
    CipFree(meta_class->class_name);
    CipFree(meta_class->services);
    CipFree(meta_class->get_single_bit_mask);
    CipFree(meta_class->set_bit_mask);
    CipFree(meta_class->get_all_bit_mask);
    CipFree(meta_class);

    /* free class data*/
    CipClass *cip_class = message_router_object_to_delete->cip_class;
    CipFree(cip_class->class_name);
    CipFree(cip_class->get_single_bit_mask);
    CipFree(cip_class->set_bit_mask);
    CipFree(cip_class->get_all_bit_mask);
    CipFree(cip_class->class_instance.attributes);
    CipFree(cip_class->services);
    CipFree(cip_class);
    /* free message router object */
    CipFree(message_router_object_to_delete);
  }
  g_first_object = NULL;
}
