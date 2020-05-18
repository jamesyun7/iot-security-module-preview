/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/*       This software is licensed under the Microsoft Software License   */
/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
/*       and in the root directory of this software.                      */
/*                                                                        */
/**************************************************************************/

#include "nx_api.h"
#include "nxd_dhcp_client.h"
#include "nxd_dns.h"
#include "nx_secure_tls_api.h"
#include "sntp_client.h"
#include "nx_driver_imxrt1062.h"
#include "board_init.h"
#include "networking.h"

/* Include the demo.  */
extern VOID demo_entry(NX_IP* ip_ptr, NX_PACKET_POOL* pool_ptr, NX_DNS* dns_ptr, ULONG (*unix_time_callback)(ULONG *unix_time));

/* Define the helper thread for running Azure SDK on ThreadX (THREADX IoT Platform).  */
#ifndef DEMO_HELPER_STACK_SIZE
#define DEMO_HELPER_STACK_SIZE      (2048)
#endif /* DEMO_HELPER_STACK_SIZE  */

#ifndef DEMO_HELPER_THREAD_PRIORITY
#define DEMO_HELPER_THREAD_PRIORITY (4)
#endif /* DEMO_HELPER_THREAD_PRIORITY  */


void* __RAM_segment_used_end__ = 0;

static TX_THREAD        demo_helper_thread;
static ULONG demo_helper_thread_stack[DEMO_HELPER_STACK_SIZE / sizeof(ULONG)];

/* Define the prototypes for demo thread.  */
static void demo_helper_thread_entry(ULONG parameter);

static ULONG unix_time_get(ULONG *unix_time);

/* Define main entry point.  */
int main(void)
{

    /* Enter the ThreadX kernel.  */
    tx_kernel_enter();
}

/* Define what the initial system looks like.  */
void    tx_application_define(void *first_unused_memory)
{

    UINT  status;

    // Initialize hardware
    board_init();

    // Initialize the network
    if (!network_init(nx_driver_imx))
    {
        printf("Failed to initialize the network\r\n");
        return;
    }

    // Start the SNTP client
    if (!sntp_start())
    {
        printf("Failed to start the SNTP client\r\n");
        return;
    }

    // Wait for an SNTP sync
    if (!sntp_wait_for_sync())
    {
        printf("Failed to start sync SNTP time\r\n");
        return;
    }
    
    /* Create demo helper thread. */
    status = tx_thread_create(&demo_helper_thread, "Demo Thread",
                              demo_helper_thread_entry, 0,
                              demo_helper_thread_stack, DEMO_HELPER_STACK_SIZE,
                              DEMO_HELPER_THREAD_PRIORITY, DEMO_HELPER_THREAD_PRIORITY, 
                              TX_NO_TIME_SLICE, TX_AUTO_START);    
    
    /* Check status.  */
    if (status)
    {
        printf("Demo helper thread creation fail: %u\r\n", status);
        return;
    }
}

/* Define demo helper thread entry.  */
void demo_helper_thread_entry(ULONG parameter)
{
    /* Use time to init the seed.  */
    srand(unix_time_get(NULL));

    /* Start demo.  */
    demo_entry(&ip_0, &main_pool, &dns_client, unix_time_get);
}

static ULONG unix_time_get(ULONG *unix_time)
{
    ULONG result = sntp_get_time();

    if (unix_time != NULL) {
        *unix_time = result;
    }

    return result;
}
