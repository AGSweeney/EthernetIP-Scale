/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/
#ifndef OPENER_USER_CONF_H_
#define OPENER_USER_CONF_H_

#include <assert.h>

#undef O_NONBLOCK
#include "typedefs.h"
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef RESTRICT
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define RESTRICT restrict
#else
#define RESTRICT
#endif
#endif

#ifndef CIP_FILE_OBJECT
  #define CIP_FILE_OBJECT 1
#endif

#ifndef CIP_SECURITY_OBJECTS
  #define CIP_SECURITY_OBJECTS 0
#endif

#ifdef OPENER_UNIT_TEST
  #include "test_assert.h"
#endif

#ifndef OPENER_IS_DLR_DEVICE
  #define OPENER_IS_DLR_DEVICE  0
#endif

#if defined(OPENER_IS_DLR_DEVICE) && 0 != OPENER_IS_DLR_DEVICE
  #define OPENER_TCPIP_IFACE_CFG_SETTABLE 1
  #define OPENER_ETHLINK_CNTRS_ENABLE     1
  #define OPENER_ETHLINK_IFACE_CTRL_ENABLE  1
  #define OPENER_ETHLINK_LABEL_ENABLE     1
  #define OPENER_ETHLINK_INSTANCE_CNT     3
#endif
#ifndef OPENER_TCPIP_IFACE_CFG_SETTABLE
  #define OPENER_TCPIP_IFACE_CFG_SETTABLE 1
#endif

#ifndef OPENER_ETHLINK_INSTANCE_CNT
  #define OPENER_ETHLINK_INSTANCE_CNT  1
#endif

#ifndef OPENER_ETHLINK_LABEL_ENABLE
  #define OPENER_ETHLINK_LABEL_ENABLE  0
#endif

#ifndef OPENER_ETHLINK_CNTRS_ENABLE
  #define OPENER_ETHLINK_CNTRS_ENABLE 1
#endif

#ifndef OPENER_ETHLINK_IFACE_CTRL_ENABLE
  #define OPENER_ETHLINK_IFACE_CTRL_ENABLE 0
#endif

#define OPENER_CIP_NUM_APPLICATION_SPECIFIC_CONNECTABLE_OBJECTS 1

#define OPENER_CIP_NUM_EXPLICIT_CONNS 6

#define OPENER_CIP_NUM_EXLUSIVE_OWNER_CONNS 1

#define OPENER_CIP_NUM_INPUT_ONLY_CONNS 1

#define OPENER_CIP_NUM_INPUT_ONLY_CONNS_PER_CON_PATH 3

#define OPENER_CIP_NUM_LISTEN_ONLY_CONNS 1

#define OPENER_CIP_NUM_LISTEN_ONLY_CONNS_PER_CON_PATH   3

#define OPENER_NUMBER_OF_SUPPORTED_SESSIONS 20

#define PC_OPENER_ETHERNET_BUFFER_SIZE 512

// LLDP Configuration
#ifndef OPENER_LLDP_ENABLED
  #define OPENER_LLDP_ENABLED 1
#endif

#ifndef OPENER_LLDP_TX_INTERVAL_MS
  #define OPENER_LLDP_TX_INTERVAL_MS 30000  // 30 seconds (standard)
#endif

static const MilliSeconds kOpenerTimerTickInMilliSeconds = 10;

#define OPENER_WITH_TRACES
#define OPENER_TRACE_LEVEL (OPENER_TRACE_LEVEL_ERROR | OPENER_TRACE_LEVEL_WARNING)

#ifndef OPENER_UNIT_TEST

#ifdef OPENER_WITH_TRACES
    #include <stdio.h>

    #define LOG_TRACE(...)  fprintf(stderr,__VA_ARGS__)

     #ifdef IDLING_ASSERT
        #define OPENER_ASSERT(assertion)                                    \
  do {                                                              \
    if( !(assertion) ) {                                            \
      LOG_TRACE("Assertion \"%s\" failed: file \"%s\", line %d\n",  \
                # assertion, __FILE__, __LINE__);                   \
      while(1) {  }                                                 \
    }                                                               \
  } while(0)

    #else
        #define OPENER_ASSERT(assertion) assert(assertion)
    #endif

#else
    #if 0
        #define OPENER_ASSERT(assertion) (assertion)
    #elif 0
        #define OPENER_ASSERT(assertion)                    \
  do { if(!(assertion) ) { while(1) {} } } while (0)
    #elif 0
        #define OPENER_ASSERT(assertion)
    #else
        #define OPENER_ASSERT(assertion) assert(assertion)
    #endif

#endif

#endif

#endif

