:: Copyright (c) Microsoft Corporation.
:: Licensed under the MIT License.

setlocal
cd /d %~dp0\..\..

:: Make sure the openocd folder was added to PATH
openocd -f tools\nxp_mimxrt1060-evk.cfg -c init -c "program _build/app/mimxrt1060_iot_security_sample"
