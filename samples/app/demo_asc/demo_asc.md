
# Overview

This demo uses C-SDK and Azure RTOS to connect to IoT Hub directly.

In this demo, we simulate the fan control application on the device. A fan can be turned on or off.


# Configure Azure IoT Hub for the demo

1. Create an IoT hub and Device using the Azure Portal.

    The  Azure IoT hub user guide can be found at: https://docs.microsoft.com/en-us/azure/iot-hub/iot-hub-create-through-portal

    Save the **IoT Hub Connection String** for later use.
    
    ![IoT Hub Configuration](../img/iothub_configuration.png)


2. Install Device Explorer.

    The installer for Device Explorer can be found at: https://github.com/Azure/azure-iot-sdk-csharp/releases/download/2019-9-11/SetupDeviceExplorer.msi


3. Run Device Explorer.

    In the “Configuration” pane, copy/paste the IoT Hub Connection String created in step1, click the “Update” button. 
    
    ![Device Explorer Configuration](../img/device_explorer/device_explorer_configuration.png) 

    The device ID can be verified in Management pane.

    ![Device Explorer Management](../img/device_explorer/device_explorer_management.png)

    In the "Management" pane, Click "SAS TokenForm" to generate the **Host Name**, **Device ID**, **Device SAS** for later use.
    ![Device Explorer Management SAS TokenForm](../img/device_explorer/device_explorer_sas_token.png)

# Configure and execute the example.

1. Refer to [README.md](../../project/x86/README.md) to generate simulator project on Windows/Linux or Click the xxx.eww project (such as: project/st/stm32f746g-disco/iar/x-ware_platform.eww) on Device in IAR.


2. Update the **Host Name**, **Device ID** and **Device SAS** as yours in *demo_asc.c*

    ```
    //
    // TODO`s: Configure core settings of application for your IoTHub.
    //

    /* Required.  */
    #define HOST_NAME                               ""
    #define DEVICE_ID                               ""

    /* Optional SAS token.  */
    #define DEVICE_SAS                              ""

    /* Optional module ID.  */
    #define MODULE_ID                               ""

    //
    // END TODO section
    //
    ```

3. Download and run the project.

    As the project runs, the demo prints out status information to the terminal IO window. The demo send UDP message every five seconds, security client will capture this message and report it to IoT Hub. Check the Terminal I/O to verify that messages have been successfully sent to the Azure IoT hub.

    ```
    DHCP In Progress...
    IP address: 192.168.1.81
    Mask: 255.255.255.0
    Gateway: 192.168.1.1
    DNS Server address: 192.168.1.1
    Connected to IoTHub!

    [INFO]ConnectionCreateSchema: local_ip=[192.168.1.81], local_port=[1234], remote_ip=[1.1.1.1], remote_port=[80], protocol=[UDP], direction=[Out]
    [INFO]ConnectionCreateSchema: local_ip=[192.168.1.81], local_port=[1234], remote_ip=[1.1.1.1], remote_port=[80], protocol=[UDP], direction=[Out]
    [INFO]ConnectionCreateSchema: local_ip=[192.168.1.81], local_port=[1234], remote_ip=[1.1.1.1], remote_port=[80], protocol=[UDP], direction=[Out]
    ```
