/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/**************************************************************************/

#include <stdio.h>
#include "nx_api.h"
#include "azrtos_iot_hub_client.h"
#include "azrtos_iot_cert.h"
#include "security_client.h"

//
// TODO`s: Configure core settings of application for your IoTHub.
//

/* Required.  */
#ifndef HOST_NAME
#define HOST_NAME                               ""
#endif /* HOST_NAME */

#ifndef DEVICE_ID
#define DEVICE_ID                               ""
#endif /* DEVICE_ID */

/* Optional SAS token.  */
#ifndef DEVICE_SAS
#define DEVICE_SAS                              ""
#endif /* DEVICE_SAS */

/* Optional module ID.  */
#ifndef MODULE_ID
#define MODULE_ID                               ""
#endif /* MODULE_ID */

//
// END TODO section
//

/* Define the AZ IOT thread stack and priority.  */
#ifndef AZRTOS_IOT_STACK_SIZE
#define AZRTOS_IOT_STACK_SIZE                       (2048)
#endif /* AZRTOS_IOT_STACK_SIZE */

#ifndef AZRTOS_IOT_THREAD_PRIORITY
#define AZRTOS_IOT_THREAD_PRIORITY                  (4)
#endif /* AZRTOS_IOT_THREAD_PRIORITY */

static ULONG azrtos_iot_thread_stack[AZRTOS_IOT_STACK_SIZE / sizeof(ULONG)];

/* Define AZ IOT TLS info.  */

/* Define the metadata size.  */
#ifndef AZRTOS_IOT_TLS_METADATA_BUFFER
#define AZRTOS_IOT_TLS_METADATA_BUFFER              (16 * 1024)
#endif /* AZRTOS_IOT_TLS_METADATA_BUFFER  */

/* Define the remote certificate count.  */
#ifndef AZRTOS_IOT_TLS_REMOTE_CERTIFICATE_COUNT
#define AZRTOS_IOT_TLS_REMOTE_CERTIFICATE_COUNT     2
#endif /* AZRTOS_IOT_TLS_REMOTE_CERTIFICATE_COUNT  */

/* Define the remote certificate buffer.  */
#ifndef AZRTOS_IOT_TLS_REMOTE_CERTIFICATE_BUFFER
#define AZRTOS_IOT_TLS_REMOTE_CERTIFICATE_BUFFER    4096
#endif /* AZRTOS_IOT_TLS_REMOTE_CERTIFICATE_BUFFER  */

/* Define the packet buffer.  */
#ifndef AZRTOS_IOT_TLS_PACKET_BUFFER
#define AZRTOS_IOT_TLS_PACKET_BUFFER                4096
#endif /* AZRTOS_IOT_TLS_PACKET_BUFFER  */

/* Define sample properties count. */
#define MAX_PROPERTY_COUNT                          2

static UCHAR azrtos_iot_tls_metadata_buffer[AZRTOS_IOT_TLS_METADATA_BUFFER];
static NX_SECURE_X509_CERT root_ca_cert;

/* Use the default ciphersuite defined in nx_secure/nx_crypto_generic_ciphersuites.c  */
extern const NX_SECURE_TLS_CRYPTO nx_crypto_tls_ciphers;

/* Define the prototypes for AZ IoT.  */
static AZRTOS_IOT                                   azrtos_iot;
static AZRTOS_IOT_HUB_CLIENT                        iothub_client;
static AZRTOS_IOT_SECURITY_CLIENT                   security_client;

static NX_UDP_SOCKET                                udp_socket;

static void init_udp(NX_IP *ip_ptr, NX_UDP_SOCKET *udp_socket);
static void send_udp_message(NX_UDP_SOCKET *udp_socket, NXD_ADDRESS address, NX_PACKET_POOL *packet_pool);

void demo_entry(NX_IP *ip_ptr, NX_PACKET_POOL *pool_ptr, NX_DNS *dns_ptr, UINT (*unix_time_callback)(ULONG *unix_time))
{
UINT  time_counter = 0;
UCHAR loop = NX_TRUE;
NXD_ADDRESS ipv4_address;

    /* Create Azure IoT handler.  */
    if (azrtos_iot_create(&azrtos_iot, (UCHAR *)"Azure IoT", ip_ptr, pool_ptr, dns_ptr, azrtos_iot_thread_stack, sizeof(azrtos_iot_thread_stack), AZRTOS_IOT_THREAD_PRIORITY))
    {
        printf("Failed on azrtos_iot_create!\r\n");
        return;
    }

    /* Initialize CA certificate. */
    if (nx_secure_x509_certificate_initialize(&root_ca_cert, (UCHAR *)_azrtos_iot_root_cert, _azrtos_iot_root_cert_size,
                                              NX_NULL, 0, NULL, 0, NX_SECURE_X509_KEY_TYPE_NONE))
    {
        printf("Failed to initialize ROOT CA certificate!");
        return;
    }

    /* Initialize IoTHub client.  */
    if (azrtos_iot_hub_client_initialize(&iothub_client, &azrtos_iot,
                                         HOST_NAME, sizeof(HOST_NAME) - 1,
                                         DEVICE_ID, sizeof(DEVICE_ID) -1,
                                         MODULE_ID, sizeof(MODULE_ID) -1,
                                         DEVICE_SAS, sizeof(DEVICE_SAS) -1,
                                         &nx_crypto_tls_ciphers,
                                         azrtos_iot_tls_metadata_buffer, sizeof(azrtos_iot_tls_metadata_buffer),
                                         &root_ca_cert))
    {
        printf("Failed on azrtos_iot_hub_client_initialize!\r\n");
        return;
    }

    /* Connect to IoTHub client. */
    if (azrtos_iot_hub_client_connect(&iothub_client, NX_FALSE, NX_WAIT_FOREVER))
    {
        printf("Failed on azrtos_iot_hub_client_connect!\r\n");
        return;
    }

    printf("Connected to IoTHub!\r\n\r\n");

    /* Init Security Client.  */
    SecurityClient_Init(&security_client, &iothub_client, "threadx-unique-machine-id", "0.0.1");

    /* Init UDP*/
    init_udp(ip_ptr, &udp_socket);

    ipv4_address.nxd_ip_version = NX_IP_VERSION_V4;
    ipv4_address.nxd_ip_address.v4 = IP_ADDRESS(1, 1, 1, 1);

    /* Loop to send UDP message.  */
    while (loop)
    {

        /* Send publish message every five seconds.  */
        if ((time_counter % 5) == 0)
        {
            send_udp_message(&udp_socket, ipv4_address, pool_ptr);
        }

        /* Sleep 1s.  */
        tx_thread_sleep(NX_IP_PERIODIC_RATE);

        /* Update the counter.  */
        time_counter++;
    }

    /* Destroy IoTHub Client.  */
    SecurityClient_Deinit(&security_client);
    azrtos_iot_hub_client_deinitialize(&iothub_client);
    azrtos_iot_delete(&azrtos_iot);
}

static void init_udp(NX_IP *ip_ptr, NX_UDP_SOCKET *udp_socket)
{
    nx_udp_socket_create(ip_ptr, udp_socket, "UDP Socket 0", NX_IP_NORMAL, NX_FRAGMENT_OKAY, 0x80, 5);
    nx_udp_socket_bind(udp_socket, 1234, TX_WAIT_FOREVER);
    nx_udp_socket_checksum_disable(udp_socket);
}

static void send_udp_message(NX_UDP_SOCKET *udp_socket, NXD_ADDRESS address, NX_PACKET_POOL *packet_pool)
{
    NX_PACKET *my_packet = NULL;
    nx_packet_allocate(packet_pool, &my_packet, NX_UDP_PACKET, TX_WAIT_FOREVER);
    nx_packet_data_append(my_packet, "DEMO UDP MESSAGE", sizeof("DEMO UDP MESSAGE") - 1, packet_pool, TX_WAIT_FOREVER);
    nxd_udp_socket_send(udp_socket, my_packet, &address, 80);
}
