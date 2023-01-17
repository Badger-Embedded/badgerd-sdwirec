/*
 *  Copyright (c) 2016 -2018 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License
 */
/**
 * @file        src/main.cpp
 * @author      Adam Malinowski <a.malinowsk2@partner.samsung.com>
 * @brief       Main sd-mux-ctrl file
 */

#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libftdi1/ftdi.h>

#define PRODUCT 0x6001
#define SAMSUNG_VENDOR 0x04e8

// SDMUX specific definitions
#define SOCKET_SEL      (0x01 << 0x00)
#define USB_SEL         (0x01 << 0x03)
#define POWER_SW_OFF    (0x01 << 0x02)
#define POWER_SW_ON     (0x01 << 0x04)
#define DYPER1          (0x01 << 0x05)
#define DYPER2          (0x01 << 0x06)

// USBMUX specific definitions
#define UM_SOCKET_SEL	(0x01 << 0x00)
#define UM_DEVICE_PWR	(0x01 << 0x01)
#define UM_DUT_LED		(0x01 << 0x02)
#define UM_GP_LED		(0x01 << 0x03)


#define DELAY_100MS     100000
#define DELAY_500MS     500000

#define CCDT_SDMUX_STR  "sd-mux"
#define CCDT_SDWIRE_STR "sd-wire"
#define CCDT_USBMUX_STR "usb-mux"

#define STRING_SIZE     128

enum CCCommand {
    CCC_List,
    CCC_DUT,
    CCC_TS,
    CCC_Tick,
    CCC_Pins,
    CCC_Info,
    CCC_ShowSerial,
    CCC_SetSerial,
    CCC_Init,
    CCC_Status,
    CCC_DyPer1,
    CCC_DyPer2,
    CCC_None
};

enum Target {
    T_DUT,
    T_TS
};

enum CCDeviceType {
    CCDT_SDMUX,
    CCDT_SDWIRE,
	CCDT_USBMUX,
    CCDT_MAX
};

enum CCFeature {
    CCF_SDMUX,
    CCF_POWERSWITCH,
    CCF_USBMUX,
    CCF_DYPERS,
    CCF_MAX
};

enum CCOption {
    CCO_DeviceId,
    CCO_DeviceSerial,
    CCO_TickTime,
    CCO_BitsInvert,
    CCO_Vendor,
    CCO_Product,
    CCO_DyPer,
    CCO_DeviceType,
    CCO_MAX
};

union CCOptionValue {
    int argn;
    char *args;
};

int doPower(bool off, bool on, CCOptionValue options[]);
int selectTarget(Target target, CCOptionValue options[]);

CCDeviceType getDeviceTypeFromString(char *deviceTypeStr) {
    if (strcmp(CCDT_SDMUX_STR, deviceTypeStr) == 0) {
        return CCDT_SDMUX;
    }

    if (strcmp(CCDT_SDWIRE_STR, deviceTypeStr) == 0) {
        return CCDT_SDWIRE;
    }

    if (strcmp(CCDT_USBMUX_STR, deviceTypeStr) == 0) {
        return CCDT_USBMUX;
    }

    return CCDT_MAX;
}

bool hasFeature(CCDeviceType deviceType, CCFeature feature) {
    static const bool featureMatrix[CCDT_MAX][CCF_MAX] = {
            {true, true, true, true},           // SD-MUX features
            {true, false, false, false},        // SDWire features
			{false, false, true, false},        // SDWire features
    };

    if (deviceType >= CCDT_MAX || feature >= CCF_MAX)
        return false;

    return featureMatrix[deviceType][feature];
}

int listDevices(CCOptionValue options[]) {
    int fret, i;
    struct ftdi_context *ftdi;
    struct ftdi_device_list *devlist, *curdev;
    char manufacturer[STRING_SIZE + 1], description[STRING_SIZE + 1], serial[STRING_SIZE + 1];
    int retval = EXIT_SUCCESS;

    if ((ftdi = ftdi_new()) == 0) {
        fprintf(stderr, "ftdi_new failed\n");
        return EXIT_FAILURE;
    }

    if ((fret = ftdi_usb_find_all(ftdi, &devlist, options[CCO_Vendor].argn, options[CCO_Product].argn)) < 0) {
        fprintf(stderr, "ftdi_usb_find_all failed: %d (%s)\n", fret, ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return EXIT_FAILURE;
    }

    if (options[CCO_DeviceId].argn == -1) {
        printf("Number of FTDI devices found: %d\n", fret);
    }

    i = 0;
    for (curdev = devlist; curdev != NULL; i++) {
        if (options[CCO_DeviceId].argn == -1 || options[CCO_DeviceId].argn == i) {
            if ((fret = ftdi_usb_get_strings(ftdi, curdev->dev, manufacturer, STRING_SIZE, description, STRING_SIZE,
                    serial, STRING_SIZE)) < 0) {
                fprintf(stderr, "ftdi_usb_get_strings failed: %d (%s)\n", fret, ftdi_get_error_string(ftdi));
                retval = EXIT_FAILURE;
                goto finish_him;
            }
            if (options[CCO_DeviceId].argn == -1) {
                printf("Dev: %d, Manufacturer: %s, Serial: %s, Description: %s\n", i,
                       manufacturer, serial, description);
            } else {
                printf("%s", serial);
            }
        }
        curdev = curdev->next;
    }

finish_him:
    ftdi_list_free(&devlist);
    ftdi_free(ftdi);

    return retval;
}

struct ftdi_context* openDevice(CCOptionValue options[], CCDeviceType *deviceType) {
    struct ftdi_context *ftdi = NULL;
    int fret;
    char product[STRING_SIZE + 1];
    CCDeviceType tmpDeviceType;

    if ((options[CCO_DeviceSerial].args == NULL) && (options[CCO_DeviceId].argn < 0)) {
        fprintf(stderr, "No serial number or device id provided!\n");
        return NULL;
    }

    if ((ftdi = ftdi_new()) == 0) {
        fprintf(stderr, "ftdi_new failed\n");
        return NULL;
    }

    if (options[CCO_DeviceSerial].args != NULL) {
        fret = ftdi_usb_open_desc_index(ftdi, options[CCO_Vendor].argn, options[CCO_Product].argn, NULL, options[CCO_DeviceSerial].args, 0);
    } else {
        fret = ftdi_usb_open_desc_index(ftdi, options[CCO_Vendor].argn, options[CCO_Product].argn, NULL, NULL, options[CCO_DeviceId].argn);
    }
    if (fret < 0) {
        fprintf(stderr, "Unable to open ftdi device: %d (%s)\n", fret, ftdi_get_error_string(ftdi));
        goto error;
    }

    fret = ftdi_read_eeprom(ftdi);
    if (fret < 0) {
        fprintf(stderr, "Unable to read ftdi eeprom: %d (%s)\n", fret, ftdi_get_error_string(ftdi));
        goto error;
    }

    fret = ftdi_eeprom_decode(ftdi, 0);
    if (fret < 0) {
        fprintf(stderr, "Unable to decode ftdi eeprom: %d (%s)\n", fret, ftdi_get_error_string(ftdi));
        goto error;
    }

    if (deviceType != NULL) {
        ftdi_eeprom_get_strings(ftdi, NULL, 0, product, sizeof(product), NULL, 0);
        tmpDeviceType = getDeviceTypeFromString(product);
        if (tmpDeviceType == CCDT_MAX) {
            fprintf(stderr, "Invalid device type. Device probably not configured!\n");
            goto error;
        }
        *deviceType = tmpDeviceType;
    }

    return ftdi;

error:
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);

    return NULL;
}

int showInfo(CCOptionValue options[]) {
    struct ftdi_context *ftdi;
    int fret, ret = EXIT_SUCCESS;

    ftdi = openDevice(options, NULL);
    if (ftdi == NULL) {
        return EXIT_FAILURE;
    }

    fret = ftdi_eeprom_decode(ftdi, 1);
    if (fret < 0) {
        fprintf(stderr, "Unable to decode ftdi eeprom: %d (%s)\n", fret, ftdi_get_error_string(ftdi));
        ret = EXIT_FAILURE;
        ftdi_usb_close(ftdi);
        ftdi_free(ftdi);
    }

    return ret;
}

int doInit(CCOptionValue options[]) {
    if (doPower(true, false, options) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    if (selectTarget(T_TS, options) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int setSerial(char *serialNumber, CCOptionValue options[]) {
    struct ftdi_context *ftdi;
    int f, ret = EXIT_FAILURE;
    char *type = options[CCO_DeviceType].args;

    if (!type) {
        fprintf(stderr, "Device type not specified\n");
        return EXIT_FAILURE;
    }

    ftdi = openDevice(options, NULL);
    if (ftdi == NULL) {
        return EXIT_FAILURE;
    }

    f = ftdi_eeprom_initdefaults(ftdi, (char *)"SRPOL", type, serialNumber);
    if (f < 0) {
        fprintf(stderr, "Unable to set eeprom strings: %d (%s)\n", f, ftdi_get_error_string(ftdi));
        goto finish_him;
    }

    f = ftdi_set_eeprom_value(ftdi, VENDOR_ID, SAMSUNG_VENDOR);
    if (f < 0) {
        fprintf(stderr, "Unable to set eeprom strings: %d (%s)\n", f, ftdi_get_error_string(ftdi));
        goto finish_him;
    }

    f = ftdi_set_eeprom_value(ftdi, PRODUCT_ID, PRODUCT);
    if (f < 0) {
        fprintf(stderr, "Unable to set eeprom strings: %d (%s)\n", f, ftdi_get_error_string(ftdi));
        goto finish_him;
    }

    if (getDeviceTypeFromString(type) == CCDT_SDWIRE) {
        f = ftdi_set_eeprom_value(ftdi, CBUS_FUNCTION_0, CBUSH_IOMODE);
        if (f < 0) {
            fprintf(stderr, "Unable to set eeprom value: %d (%s)\n", f, ftdi_get_error_string(ftdi));
            goto finish_him;
        }
    }

    if (getDeviceTypeFromString(type) == CCDT_USBMUX) {
        f = ftdi_set_eeprom_value(ftdi, CBUS_FUNCTION_0, CBUSH_IOMODE);
        if (f < 0) {
            fprintf(stderr, "Unable to set eeprom value: %d (%s)\n", f, ftdi_get_error_string(ftdi));
            goto finish_him;
        }
        f = ftdi_set_eeprom_value(ftdi, CBUS_FUNCTION_1, CBUSH_IOMODE);
        if (f < 0) {
            fprintf(stderr, "Unable to set eeprom value: %d (%s)\n", f, ftdi_get_error_string(ftdi));
            goto finish_him;
        }
        f = ftdi_set_eeprom_value(ftdi, CBUS_FUNCTION_2, CBUSH_IOMODE);
        if (f < 0) {
            fprintf(stderr, "Unable to set eeprom value: %d (%s)\n", f, ftdi_get_error_string(ftdi));
            goto finish_him;
        }
        f = ftdi_set_eeprom_value(ftdi, CBUS_FUNCTION_3, CBUSH_IOMODE);
        if (f < 0) {
            fprintf(stderr, "Unable to set eeprom value: %d (%s)\n", f, ftdi_get_error_string(ftdi));
            goto finish_him;
        }
    }

    f = ftdi_eeprom_build(ftdi);
    if (f < 0) {
        fprintf(stderr, "Unable to build eeprom: %d (%s)\n", f, ftdi_get_error_string(ftdi));
        goto finish_him;
    }

    f = ftdi_write_eeprom(ftdi);
    if (f < 0) {
        fprintf(stderr, "Unable to write eeprom into device: %d (%s)\n", f, ftdi_get_error_string(ftdi));
        goto finish_him;
    }

    ret = EXIT_SUCCESS;

finish_him:
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);

    return ret;
}

int writePins(struct ftdi_context *ftdi, unsigned char pins) {
    int f = ftdi_write_data(ftdi, &pins, 1);
    if (f < 0) {
        fprintf(stderr,"write failed for 0x%x, error %d (%s)\n", pins, f, ftdi_get_error_string(ftdi));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

struct ftdi_context* prepareDevice(CCOptionValue options[], unsigned char *pins, CCDeviceType *deviceType) {
    struct ftdi_context *ftdi;
    int f;

    ftdi = openDevice(options, deviceType);
    if (ftdi == NULL) {
        return NULL;
    }

    if (*deviceType == CCDT_SDWIRE || *deviceType == CCDT_USBMUX) {
        return ftdi; // None of the following steps need to be performed for this type of device.
    }

    f = ftdi_set_bitmode(ftdi, 0xFF, BITMODE_BITBANG);
    if (f < 0) {
        fprintf(stderr, "Unable to enable bitbang mode: %d (%s)\n", f, ftdi_get_error_string(ftdi));
        ftdi_usb_close(ftdi);
        ftdi_free(ftdi);
        return NULL;
    }

    if (pins != NULL) {
        f = ftdi_read_data(ftdi, pins, 1);
        if (f < 0) {
            fprintf(stderr,"read failed, error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            ftdi_usb_close(ftdi);
            ftdi_free(ftdi);
            return NULL;
        }
    }

    return ftdi;
}

int powerOff(struct ftdi_context *ftdi, unsigned char *pins) {
    // Turn on the coil
    *pins |= POWER_SW_ON;
    *pins &= ~(POWER_SW_OFF);
    if (writePins(ftdi, *pins) != EXIT_SUCCESS)
        return EXIT_FAILURE;

    // Wait for 100ms
    usleep(DELAY_100MS);

    // Turn off the coil
    *pins |= POWER_SW_OFF;
    if (writePins(ftdi, *pins) != EXIT_SUCCESS)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int powerOn(struct ftdi_context *ftdi, unsigned char *pins) {
    // Turn on the coil
    *pins |= POWER_SW_OFF;
    *pins &= ~(POWER_SW_ON);
    if (writePins(ftdi, *pins) != EXIT_SUCCESS)
        return EXIT_FAILURE;

    // Wait for 100ms
    usleep(DELAY_100MS);

    // Turn off the coil
    *pins |= POWER_SW_ON;
    if (writePins(ftdi, *pins) != EXIT_SUCCESS)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int doPower(bool off, bool on, CCOptionValue options[]) {
    unsigned char pins;
    CCDeviceType deviceType;
    int ret = EXIT_SUCCESS;

    int period = 1000;
    if (options[CCO_TickTime].argn > 0) {
        period = options[CCO_TickTime].argn;
    }

    struct ftdi_context *ftdi = prepareDevice(options, &pins, &deviceType);
    if (ftdi == NULL)
        return EXIT_FAILURE;

    if (!hasFeature(deviceType, CCF_POWERSWITCH)) {
        fprintf(stderr,"Power switching is not available on this device.\n");
        ret = EXIT_FAILURE;
        goto finish_him;
    }

    ret = powerOff(ftdi, &pins);
    if (off && (ret != EXIT_SUCCESS)) {
        ret = EXIT_FAILURE;
        goto finish_him;
    }

    // Wait for specified period in ms
    usleep(period * 1000);

    ret = powerOn(ftdi, &pins);
    if (on && (ret != EXIT_SUCCESS)) {
        ret = EXIT_FAILURE;
        goto finish_him;
    }

finish_him:
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);

    return ret;
}

int selectTarget(Target target, CCOptionValue options[]) {
    unsigned char pins;
    CCDeviceType deviceType;
    int ret = EXIT_SUCCESS;

    struct ftdi_context *ftdi = prepareDevice(options, &pins, &deviceType);
    if (ftdi == NULL)
        return EXIT_FAILURE;

    if (deviceType == CCDT_SDWIRE) {
        unsigned char pinState = 0x00;
        pinState |= 0xF0; // Upper half of the byte sets all pins to output (SDWire has only one bit - 0)
        pinState |= target == T_DUT ? 0x00 : 0x01; // Lower half of the byte sets state of output pins.
                                                   // In this particular case we care only of bit 0.
        ret |= ftdi_set_bitmode(ftdi, pinState, BITMODE_CBUS);
        goto finish_him;
    }

    if (deviceType == CCDT_USBMUX) {
        unsigned char pinState = 0xF0;

        if (target == T_DUT) {
            pinState &= ~UM_DEVICE_PWR;
            ret |= ftdi_set_bitmode(ftdi, pinState, BITMODE_CBUS);
            usleep(DELAY_500MS);
            pinState |= UM_DEVICE_PWR;
            ret |= ftdi_set_bitmode(ftdi, pinState, BITMODE_CBUS);
            usleep(DELAY_100MS);
            pinState |= UM_DUT_LED;
            pinState &= ~UM_SOCKET_SEL;
            pinState &= ~UM_GP_LED;
            ret |= ftdi_set_bitmode(ftdi, pinState, BITMODE_CBUS);
        } else {
            pinState &= ~UM_DUT_LED;
            pinState &= ~UM_DEVICE_PWR;
            ret |= ftdi_set_bitmode(ftdi, pinState, BITMODE_CBUS);
            usleep(DELAY_500MS);
            pinState |= UM_DEVICE_PWR;
            ret |= ftdi_set_bitmode(ftdi, pinState, BITMODE_CBUS);
            usleep(DELAY_100MS);
            pinState |= UM_SOCKET_SEL;
            pinState |= UM_GP_LED;
            ret |= ftdi_set_bitmode(ftdi, pinState, BITMODE_CBUS);
        }

        goto finish_him;
    }

    // Currently only old SD-MUX is the other device so do the job in its style.
    if (target == T_DUT) {
        pins &= ~(USB_SEL);
        pins &= ~(SOCKET_SEL);
        if (powerOn(ftdi, &pins) != EXIT_SUCCESS) {  // Also selects USB and SD
            ret = EXIT_FAILURE;
            goto finish_him;
        }
    } else {
        pins |= USB_SEL;
        pins |= SOCKET_SEL;
        if (powerOff(ftdi, &pins) != EXIT_SUCCESS) { // Also selects USB and SD
            ret = EXIT_FAILURE;
            goto finish_him;
        }
    }

finish_him:
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);

    return ret;
}

int setPins(unsigned char pins, CCOptionValue options[]) {
    CCDeviceType deviceType;
    struct ftdi_context *ftdi = prepareDevice(options, NULL, &deviceType);
    if (ftdi == NULL)
        return EXIT_FAILURE;

    if (options[CCO_DeviceSerial].args) {
        pins = ~pins;
    }

    if (deviceType == CCDT_SDWIRE) {
        // SDWire has only one pin already controlled by selectTarget function.
        // There is no use to repeat this functionality here.
        return EXIT_FAILURE;
    }

    printf("Write data: 0x%x\n", pins);

    int ret = writePins(ftdi, pins);

    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);

    return ret;
}

int showStatus(CCOptionValue options[]) {
	int ret = 0;
    unsigned char pins;
    CCDeviceType deviceType;
    struct ftdi_context *ftdi = prepareDevice(options, &pins, &deviceType);
    if (ftdi == NULL)
        return EXIT_FAILURE;


    if (deviceType == CCDT_SDWIRE) {
       if (ftdi_read_pins(ftdi, &pins) != 0) {
           fprintf(stderr, "Error reading pins state.\n");
           ret = EXIT_FAILURE;
           goto finish_him;
       }
       fprintf(stdout, "SD connected to: %s\n", pins & SOCKET_SEL ? "TS" : "DUT");
       goto finish_him;
    }

    if (deviceType == CCDT_USBMUX) {
       if (ftdi_read_pins(ftdi, &pins) != 0) {
           fprintf(stderr, "Error reading pins state.\n");
           ret = EXIT_FAILURE;
           goto finish_him;
       }

       if (pins == 0xff) {
           fprintf(stdout, "Device not initialized!\n");
           goto finish_him;
       }

       fprintf(stdout, "SD connected to: %s\n", pins & UM_SOCKET_SEL ? "TS" : "DUT");
       goto finish_him;
    }

    // Currently only old SD-MUX is the other device so do the job in its style.
    if (!((pins & POWER_SW_ON) && (pins & POWER_SW_OFF))) {
        fprintf(stdout, "Device not initialized!\n");
        goto finish_him;
    }

    fprintf(stdout, "USB connected to: %s\n", pins & USB_SEL ? "TS" : "DUT");
    fprintf(stdout, "SD connected to: %s\n", pins & SOCKET_SEL ? "TS" : "DUT");

finish_him:
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);

    return ret;
}

int setDyPer(CCCommand cmd, CCOptionValue options[]) {
    unsigned char pins;
    bool switchOn;
    CCDeviceType deviceType;
    int ret = EXIT_SUCCESS, dyper;

    struct ftdi_context *ftdi = prepareDevice(options, &pins, &deviceType);
    if (ftdi == NULL)
        return EXIT_FAILURE;

    if (!hasFeature(deviceType, CCF_DYPERS)) {
        fprintf(stderr,"DyPers are not available on this device.\n");
        return EXIT_FAILURE;
    }

    #define STRON "ON"
    #define STROFF "OFF"

    if (strcasecmp(STRON, options[CCO_DyPer].args) == 0) {
      switchOn = true;
    } else if (strcasecmp(STROFF, options[CCO_DyPer].args) == 0) {
      switchOn = false;
    } else {
      fprintf(stderr,"Invalid DyPer argument! Use \"on\" or \"off\".\n");
      goto finish_him;
    }

    dyper = cmd == CCC_DyPer1 ? DYPER1 : DYPER2;
    pins = switchOn ? pins | dyper : pins & ~dyper;

    if (writePins(ftdi, pins) != EXIT_SUCCESS)
        ret = EXIT_FAILURE;

finish_him:
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);

    return ret;
}

int parseArguments(int argc, const char **argv, CCCommand *cmd, int *arg, char *args, size_t argsLen,
                   CCOptionValue options[]) {
    int c;
    char *serial = NULL;

    poptContext optCon;
    struct poptOption optionsTable[] = {
            // Commands
            { "list", 'l', POPT_ARG_NONE, NULL, 'l', "lists all sd-mux devices connected to PC", NULL },
            { "info", 'i', POPT_ARG_NONE, NULL, 'i', "displays info about device", "serial number" },
            { "show-serial", 'o', POPT_ARG_NONE, NULL, 'o', "displays serial number of given device", NULL },
            { "set-serial", 'r', POPT_ARG_STRING, &serial, 'r', "writes serial number to given device", NULL },
            { "init", 't', POPT_ARG_NONE, NULL, 't', "initialize target board", NULL },
            { "dut", 'd', POPT_ARG_NONE, NULL, 'd', "connects SD card and USB to the target board", NULL },
            { "ts", 's', POPT_ARG_NONE, NULL, 's', "connects SD card and USB to the test server", NULL },
            { "pins", 'p', POPT_ARG_INT, arg, 'p', "write pin state in bitbang mode", NULL },
            { "tick", 'c', POPT_ARG_NONE, NULL, 'c', "turn off and on power supply of DUT", NULL },
            { "status", 'u', POPT_ARG_NONE, NULL, 'u', "show current status: DUT or TS or NOINIT", NULL },
            { "dyper1", 'y', POPT_ARG_STRING, &options[CCO_DyPer].args, 'y', "Connect or disconnect terminals of 1st dynamic jumper; STRING = \"on\" or \"off\"", NULL },
            { "dyper2", 'z', POPT_ARG_STRING, &options[CCO_DyPer].args, 'z', "Connect or disconnect terminals of 2nd dynamic jumper; STRING = \"on\" or \"off\"", NULL },
            // Options
            { "tick-time", 'm', POPT_ARG_INT, &options[CCO_TickTime].argn, 'm', "set time delay for 'tick' command",
                    NULL },
            { "device-id", 'v', POPT_ARG_INT, &options[CCO_DeviceId].argn, 'v', "use device with given id", NULL },
            { "device-serial", 'e', POPT_ARG_STRING, &options[CCO_DeviceSerial].args, 'e',
                    "use device with given serial number", NULL },
            { "device-type", 'k', POPT_ARG_STRING, &options[CCO_DeviceType].args, 'k',
                    "make the device of this type", NULL },
            { "vendor", 'x', POPT_ARG_INT, &options[CCO_Vendor].argn, 'x', "use device with given vendor id", NULL },
            { "product", 'a', POPT_ARG_INT, &options[CCO_Product].argn, 'a', "use device with given product id", NULL },
            { "invert", 'n', POPT_ARG_NONE, NULL, 'n', "invert bits for --pins command", NULL },
            POPT_AUTOHELP
            { NULL, 0, 0, NULL, 0, NULL, NULL }
    };

    optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
    poptSetOtherOptionHelp(optCon, "command");
    if (argc < 2) {
        poptPrintUsage(optCon, stderr, 0);
        poptFreeContext(optCon);
        return EXIT_SUCCESS;
    }
    /* Now do options processing, get portname */
    while ((c = poptGetNextOpt(optCon)) >= 0) {
        switch (c) {
            case 'l':
                *cmd = CCC_List;
                break;
            case 'i':
                *cmd = CCC_Info;
                break;
            case 'o':
                *cmd = CCC_ShowSerial;
                break;
            case 'r':
                *cmd = CCC_SetSerial;
                break;
            case 't':
                *cmd = CCC_Init;
                break;
            case 'd':
                *cmd = CCC_DUT;
                break;
            case 's':
                *cmd = CCC_TS;
                break;
            case 'p':
                *cmd = CCC_Pins;
                break;
            case 'c':
                *cmd = CCC_Tick;
                break;
            case 'u':
                *cmd = CCC_Status;
                break;
            case 'y':
                *cmd = CCC_DyPer1;
                break;
            case 'z':
                *cmd = CCC_DyPer2;
                break;
            case 'n':
                options[CCO_BitsInvert].argn = 1;
                break;
        }
    }

    if (serial)
        snprintf(args, argsLen, "%s", serial);
    free(serial);

    if (c < -1) {
        fprintf(stderr, "%s: %s\n", poptBadOption(optCon, POPT_BADOPTION_NOALIAS), poptStrerror(c));
        poptFreeContext(optCon);
        return EXIT_FAILURE;
    }

    poptFreeContext(optCon);

    return EXIT_SUCCESS;
}

int main(int argc, const char **argv) {
    CCCommand cmd = CCC_None;
    int arg;
    char args[64];
    CCOptionValue options[CCO_MAX];
    memset(&options, 0, sizeof(options));
    options[CCO_DeviceId].argn = -1;
    options[CCO_Vendor].argn = SAMSUNG_VENDOR;
    options[CCO_Product].argn = PRODUCT;

    if (parseArguments(argc, argv, &cmd, &arg, args, sizeof(args), options) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    switch (cmd) {
    case CCC_None:
        fprintf(stderr, "No command specified\n");
        return EXIT_FAILURE;
    case CCC_List:
        return listDevices(options);
    case CCC_Info:
        return showInfo(options);
    case CCC_ShowSerial:
        return listDevices(options);
    case CCC_SetSerial:
        return setSerial(args, options);
    case CCC_Init:
        return doInit(options);
    case CCC_DUT:
        return selectTarget(T_DUT, options);
    case CCC_TS:
        return selectTarget(T_TS, options);
    case CCC_Tick:
        return doPower(true, true, options);
    case CCC_Pins:
        return setPins((unsigned char)arg, options);
    case CCC_DyPer1:
    case CCC_DyPer2:
        return setDyPer(cmd, options);
    case CCC_Status:
        return showStatus(options);
    }

    return EXIT_SUCCESS;
}
