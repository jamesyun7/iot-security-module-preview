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
#include "nx_secure_tls_api.h"
#include "nxd_mqtt_client.h"
#include "nxd_dns.h"
#include "nx_azure_iot_cert.h"
#include "asc_security_azurertos/security_module_mqtt.h"

#ifdef NXD_MQTT_CLOUD_ENABLE
#include "nx_cloud.h"
#else/* NXD_MQTT_CLOUD_ENABLE */
#error "IoT Security Module Sample requires NXD_MQTT_CLOUD_ENABLE, abort"
#endif /* NXD_MQTT_CLOUD_ENABLE */

//
// TODO`s: Configure core settings of application for your IoTHub, replace the [HOST Name] and [Device ID] as yours. Use Device Explorer to generate [SAS].
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

//
// END TODO section
//

#define SERVER_PORT                             8883

#define MQTT_USERNAME                               HOST_NAME "/" DEVICE_ID
#define MQTT_MQTT_PUBLISH_TOPIC_WITH_ASC_IFID       "devices/" DEVICE_ID "/messages/events/%%24.ct=application%%2Fjson&%%24.ce=utf-8&%%24.ifid=urn%%3Aazureiot%%3ASecurity%%3ASecurityAgent%%3A1"
#define MQTT_PUBLISH_TOPIC                          MQTT_MQTT_PUBLISH_TOPIC_WITH_ASC_IFID
#define MQTT_SUBSCRIBE_TOPIC                        "devices/" DEVICE_ID "/messages/devicebound/#"


/* Define the default metadata, remote certificate, packet buffer, etc.
The user can override this
   via -D command line option or via project settings.  */

/* Define the timeout for establishing TLS connection, default 40s. NX_IP_PERIODIC_RATE indicate one second.  */
#ifndef THREADX_TLS_WAIT_OPTION
#define THREADX_TLS_WAIT_OPTION                 (40 * NX_IP_PERIODIC_RATE)
#endif /* THREADX_TLS_WAIT_OPTION  */

/* Define the timeout for sending TLS packet, default 1s. NX_IP_PERIODIC_RATE indicate one second.  */
#ifndef THREADX_TLS_SEND_WAIT_OPTION
#define THREADX_TLS_SEND_WAIT_OPTION            (NX_IP_PERIODIC_RATE)
#endif /* THREADX_TLS_SEND_WAIT_OPTION  */

/* Define the metadata size for THREADX TLS.  */
#ifndef THREADX_TLS_METADATA_BUFFER
#define THREADX_TLS_METADATA_BUFFER             (16 * 1024)
#endif /* THREADX_TLS_METADATA_BUFFER  */

/* Define the remote certificate count for THREADX TLS.  */
#ifndef THREADX_TLS_REMOTE_CERTIFICATE_COUNT
#define THREADX_TLS_REMOTE_CERTIFICATE_COUNT    2
#endif /* THREADX_TLS_REMOTE_CERTIFICATE_COUNT  */

/* Define the remote certificate buffer for THREADX TLS.  */
#ifndef THREADX_TLS_REMOTE_CERTIFICATE_BUFFER
#define THREADX_TLS_REMOTE_CERTIFICATE_BUFFER   4096
#endif /* THREADX_TLS_REMOTE_CERTIFICATE_BUFFER  */

/* Define the packet buffer for THREADX TLS.  */
#ifndef THREADX_TLS_PACKET_BUFFER
#define THREADX_TLS_PACKET_BUFFER               4096
#endif /* THREADX_TLS_PACKET_BUFFER  */

/* Define MQTT topic length */
#ifndef MQTT_TOPIC_NAME_LENGTH
#define MQTT_TOPIC_NAME_LENGTH                  200
#endif /* MQTT_TOPIC_NAME_LENGTH */
/* Define MQTT message length */
#ifndef MQTT_MESSAGE_NAME_LENGTH
#define MQTT_MESSAGE_NAME_LENGTH                200
#endif /* MQTT_MESSAGE_NAME_LENGTH */

static UCHAR threadx_tls_metadata_buffer[THREADX_TLS_METADATA_BUFFER];
static NX_SECURE_X509_CERT threadx_tls_remote_certificate[THREADX_TLS_REMOTE_CERTIFICATE_COUNT];
static UCHAR threadx_tls_remote_cert_buffer[THREADX_TLS_REMOTE_CERTIFICATE_COUNT][THREADX_TLS_REMOTE_CERTIFICATE_BUFFER];
static UCHAR threadx_tls_packet_buffer[THREADX_TLS_PACKET_BUFFER];

/* Use the default ciphersuite defined in nx_secure/nx_crypto_generic_ciphersuites.c  */
extern const NX_SECURE_TLS_CRYPTO nx_crypto_tls_ciphers;

static NX_CLOUD cloud;

/* MQTT.  */
static NXD_MQTT_CLIENT  mqtt_client_secure;
static UCHAR            mqtt_client_stack[4096];
static UCHAR            mqtt_topic_buffer[MQTT_TOPIC_NAME_LENGTH];
static UINT             mqtt_topic_length;
static UCHAR            mqtt_message_buffer[MQTT_MESSAGE_NAME_LENGTH];
static UINT             mqtt_message_length;

/* ASC Security Module */
static AZRTOS_IOT_SECURITY_MODULE security_module;

/* Fan info.  */
static UINT fan_on = NX_FALSE;
static UINT temperature = 20;

/* Process command.  */
static VOID my_notify_func(NXD_MQTT_CLIENT* client_ptr, UINT number_of_messages)
{

UINT status;
UINT i;

    /* Get the mqtt client message.  */
    status = nxd_mqtt_client_message_get(
        &mqtt_client_secure,
        mqtt_topic_buffer,
        sizeof(mqtt_topic_buffer),
        &mqtt_topic_length,
        mqtt_message_buffer,
        sizeof(mqtt_message_buffer),
        &mqtt_message_length
    );
    if(status == NXD_MQTT_SUCCESS)
    {
        mqtt_topic_buffer[mqtt_topic_length] = 0;
        mqtt_message_buffer[mqtt_message_length] = 0;
        printf("[Received]  topic = %s, message = %s\r\n", mqtt_topic_buffer, mqtt_message_buffer);

        /* To lowcase. */
        for (i = 0; i < mqtt_message_length; i++)
        {
            if ((mqtt_message_buffer[i] >= 'A') && (mqtt_message_buffer[i] <= 'Z'))
            {
                mqtt_message_buffer[i] = mqtt_message_buffer[i] - 'A' + 'a';
            }
        }

        if (strstr((CHAR *)mqtt_message_buffer, "fan_on"))
        {
            if (strstr((CHAR *)mqtt_message_buffer, "true"))
            {
                fan_on = NX_TRUE;
            }
            else if (strstr((CHAR *)mqtt_message_buffer, "false"))
            {
                fan_on = NX_FALSE;
            }
        }
    }

    return;
}

static UINT threadx_mqtt_tls_setup(NXD_MQTT_CLIENT *client_ptr, NX_SECURE_TLS_SESSION *tls_session,
                                   NX_SECURE_X509_CERT *certificate, NX_SECURE_X509_CERT *trusted_certificate)
{
UINT i;

    for (i = 0; i < sizeof(threadx_tls_remote_certificate) / sizeof(NX_SECURE_X509_CERT); i++)
    {

        /* Need to allocate space for the certificate coming in from the remote host. */
        nx_secure_tls_remote_certificate_allocate(
            tls_session,
            &threadx_tls_remote_certificate[i],
            threadx_tls_remote_cert_buffer[i],
            sizeof(threadx_tls_remote_cert_buffer[i])
        );
    }

    /* Add a CA Certificate to our trusted store for verifying incoming server certificates. */
        nx_secure_tls_remote_certificate_allocate(
            tls_session,
            &threadx_tls_remote_certificate[i],
            threadx_tls_remote_cert_buffer[i],
            sizeof(threadx_tls_remote_cert_buffer[i])
        );

    nx_secure_tls_trusted_certificate_add(
        tls_session,
        trusted_certificate
    );

    nx_secure_tls_session_packet_buffer_set(
        tls_session,
        threadx_tls_packet_buffer,
        sizeof(threadx_tls_packet_buffer)
    );

    return(NX_SUCCESS);
}

VOID demo_entry(NX_IP *ip_ptr, NX_PACKET_POOL *pool_ptr, NX_DNS *dns_ptr, ULONG (*unix_time_callback)(ULONG *unix_time))
{
UINT        status;
UINT        time_counter = 0;
NXD_ADDRESS server_address;

    /* Create MQTT Client.  */
    status = nx_cloud_create(
        &cloud,
        "Cloud Helper",
        (VOID*)mqtt_client_stack,
        sizeof(mqtt_client_stack),
        2
    );
    status += _nxd_mqtt_client_cloud_create(
        &mqtt_client_secure,
        "MQTT_CLIENT",
        DEVICE_ID,
        strlen(DEVICE_ID),
        ip_ptr,
        pool_ptr,
        &cloud
    );

    /* Check status.  */
    if (status)
    {
        printf("Error in creating MQTT client: 0x%02x\r\n", status);
        return;
    }

    /* Create TLS session.  */
    status = nx_secure_tls_session_create(
        &(mqtt_client_secure.nxd_mqtt_tls_session),
        &nx_crypto_tls_ciphers,
        threadx_tls_metadata_buffer,
        THREADX_TLS_METADATA_BUFFER
    );

    /* Check status.  */
    if (status)
    {
        printf("Error in creating TLS Session: 0x%02x\r\n", status);
        return;
    }

    /* Set username and password.  */
    status = nxd_mqtt_client_login_set(
        &mqtt_client_secure,
        MQTT_USERNAME,
        strlen(MQTT_USERNAME),
        DEVICE_SAS,
        strlen(DEVICE_SAS)
    );

    /* Check status.  */
    if (status)
    {
        printf("Error in setting username and password: 0x%02x\r\n", status);
        return;
    }

    /* Resolve the server name to get the address.  */
    status = nxd_dns_host_by_name_get(
        dns_ptr,
        (UCHAR *)HOST_NAME,
        &server_address,
        NX_IP_PERIODIC_RATE,
        NX_IP_VERSION_V4
    );

    if (status)
    {
        printf("Error in getting host address: 0x%02x\r\n", status);
        return;
    }

    /* Start MQTT connection.  */
    status = nxd_mqtt_client_secure_connect(
        &mqtt_client_secure, &server_address,
        SERVER_PORT,
        threadx_mqtt_tls_setup,
        6 * NX_IP_PERIODIC_RATE,
        NX_TRUE,
        NX_WAIT_FOREVER
    );

    if (status)
    {
        printf("Error in connecting to server: 0x%02x\r\n", status);
        return;
    }

    printf("Connected to server\r\n");

    /* Subscribe.  */
    status = nxd_mqtt_client_subscribe(
        &mqtt_client_secure,
        MQTT_SUBSCRIBE_TOPIC,
        strlen(MQTT_SUBSCRIBE_TOPIC),
        0
    );

    if (status)
    {
        printf("Error in subscribing to server: 0x%02x\r\n", status);
        return;
    }

    printf("Subscribed to server\r\n");

    /* Set notify function.  */
    status = nxd_mqtt_client_receive_notify_set(
        &mqtt_client_secure,
        my_notify_func
    );

    if (status)
    {
        printf("Error in setting receive notify: 0x%02x\r\n", status);
        return;
    }

    status = security_module_mqtt_init(
        &security_module,
        &mqtt_client_secure,
        MQTT_PUBLISH_TOPIC,
        (unix_time_callback_t)unix_time_callback
    );

    if (status)
    {
        printf("Error in creating security module: 0x%02x\r\n", status);
        return;
    } else {
        printf("Initialized Security Module successfully\r\n");
    }

    /* Loop to send publish message and wait for command.  */
    while (1)
    {

        /* calculate temperature every five seconds.  */
        if ((time_counter % 5) == 0)
        {

            /* Check if turn fan on.  */
            if (fan_on == NX_FALSE)
            {
                if (temperature < 40)
                    temperature++;
            }
            else
            {
                if (temperature > 0)
                    temperature--;
            }

            /* Publish message.  */
            printf("[User Application] {\"temperature\": %d}\r\n", temperature);
        }

        /* Sleep 1s.  */
        tx_thread_sleep(100);

        /* Update the counter.  */
        time_counter++;
    }

    security_module_mqtt_deinit(&security_module);
}

