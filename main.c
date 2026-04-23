/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the USB Device CDC echo Example
*              for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* $ Copyright YEAR Cypress Semiconductor $
*******************************************************************************/

// Reference: https://community.infineon.com/t5/Knowledge-Base-Articles/Contactless-vital-signs-monitoring-with-XENSIV-BGT60TR13C-radar-sensor-A-MATLAB/ta-p/1193238
// Radar: https://github.com/Infineon/sensor-xensiv-bgt60trxx/blob/master/README.md
// USB: https://github.com/Infineon/mtb-example-usb-device-cdc-echo/blob/master/README.md

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "xensiv_bgt60trxx_mtb.h"
#include "USB.h"
#include "USB_CDC.h"
#include <stdio.h>

#define XENSIV_BGT60TRXX_CONF_IMPL
#include "vital_sensing_radar_settings.h"
#include "resource_map.h"


/*******************************************************************************
* Macros
********************************************************************************/
#define USB_CONFIG_DELAY          (50U) /* In milliseconds */

#define CHUNK_SIZE 128 /* read 128 samples from radar FIFO every time */

#define XENSIV_BGT60TRXX_SPI_FREQUENCY      (25000000UL)

#define NUM_SAMPLES_PER_FRAME               (XENSIV_BGT60TRXX_CONF_NUM_RX_ANTENNAS *\
                                             XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME *\
                                             XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP)


#define MAGIC (0xFFDDFFDD)

struct __attribute__((packed)) hdr {
    uint32_t magic;
    uint32_t length;
};

/*******************************************************************************
* Function Prototypes
********************************************************************************/
void usb_add_cdc(void);

/*********************************************************************
*       Information that are used during enumeration
**********************************************************************/
static const USB_DEVICE_INFO usb_deviceInfo = {
    0x058B,                       /* VendorId    */
    0x027D,                       /* ProductId    */
    "Infineon Technologies",      /* VendorName   */
    "Vital Sense Serial",         /* ProductName  */
    "2439"                        /* SerialNumber */
};


/*******************************************************************************
* Global Variables
********************************************************************************/

// USB Related variables
static USB_CDC_HANDLE usb_cdcHandle;

// Radar related variables
static cyhal_spi_t cyhal_spi;
static xensiv_bgt60trxx_mtb_t sensor;
static volatile bool data_available = false;

/* Allocate enough memory for the radar dara frame. */
static struct hdr header = { .magic = MAGIC, .length = CHUNK_SIZE * 2 };

static uint8_t tx_buffer[sizeof(struct hdr) + CHUNK_SIZE * 2];

/* Interrupt handler to react on sensor indicating the availability of new data */
void xensiv_bgt60trxx_mtb_interrupt_handler(void *args, cyhal_gpio_event_t event) {
    CY_UNUSED_PARAMETER(args);
    CY_UNUSED_PARAMETER(event);
    data_available = true;
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM4 CPU.
*
*  1. It initializes the USB Device block
*  and enumerates as a CDC device.
*
*  2. It recevies data from host
*  and echos it back
*
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void) {
    cy_rslt_t result;

    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS) { CY_ASSERT(0); }

    __enable_irq();

    /********************************************************************************
    * USB Setup
    ********************************************************************************/
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);
    cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    printf("\x1b[2J\x1b[;H");
    printf("****************** Vital Sense Application ****************** \r\n\n");

    USBD_Init();
    usb_add_cdc();
    USBD_SetDeviceInfo(&usb_deviceInfo);
    USBD_Start();

    while ((USBD_GetState() & USB_STAT_CONFIGURED) != USB_STAT_CONFIGURED) {
        cyhal_system_delay_ms(USB_CONFIG_DELAY);
    }

    /********************************************************************************
    * Radar Setup
    ********************************************************************************/

    /* Initialize the SPI interface to BGT60. */
    result = cyhal_spi_init(&cyhal_spi,
                            PIN_XENSIV_BGT60TRXX_SPI_MOSI,
                            PIN_XENSIV_BGT60TRXX_SPI_MISO,
                            PIN_XENSIV_BGT60TRXX_SPI_SCLK,
                            NC,
                            NULL,
                            8,
                            CYHAL_SPI_MODE_00_MSB,
                            false);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* Reduce drive strength to improve EMI */
    Cy_GPIO_SetSlewRate(CYHAL_GET_PORTADDR(PIN_XENSIV_BGT60TRXX_SPI_MOSI),
                        CYHAL_GET_PIN(PIN_XENSIV_BGT60TRXX_SPI_MOSI), CY_GPIO_SLEW_FAST);
    Cy_GPIO_SetDriveSel(CYHAL_GET_PORTADDR(PIN_XENSIV_BGT60TRXX_SPI_MOSI),
                        CYHAL_GET_PIN(PIN_XENSIV_BGT60TRXX_SPI_MOSI), CY_GPIO_DRIVE_1_8);
    Cy_GPIO_SetSlewRate(CYHAL_GET_PORTADDR(PIN_XENSIV_BGT60TRXX_SPI_SCLK),
                        CYHAL_GET_PIN(PIN_XENSIV_BGT60TRXX_SPI_SCLK), CY_GPIO_SLEW_FAST);
    Cy_GPIO_SetDriveSel(CYHAL_GET_PORTADDR(PIN_XENSIV_BGT60TRXX_SPI_SCLK),
                        CYHAL_GET_PIN(PIN_XENSIV_BGT60TRXX_SPI_SCLK), CY_GPIO_DRIVE_1_8);

    /* Set SPI data rate to communicate with sensor */
    result = cyhal_spi_set_frequency(&cyhal_spi, XENSIV_BGT60TRXX_SPI_FREQUENCY);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* Wait LDO stable */
    (void)cyhal_system_delay_ms(5);

    result = xensiv_bgt60trxx_mtb_init(&sensor,
                                       &cyhal_spi,
                                       PIN_XENSIV_BGT60TRXX_SPI_CSN,
                                       PIN_XENSIV_BGT60TRXX_RSTN,
                                       register_list,
                                       XENSIV_BGT60TRXX_CONF_NUM_REGS);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* The sensor will generate an interrupt once the sensor FIFO level is
       NUM_SAMPLES_PER_FRAME */
    result = xensiv_bgt60trxx_mtb_interrupt_init(&sensor,
                                                 CHUNK_SIZE,
                                                 PIN_XENSIV_BGT60TRXX_IRQ,
                                                 CYHAL_ISR_PRIORITY_DEFAULT,
                                                 xensiv_bgt60trxx_mtb_interrupt_handler,
                                                 NULL);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    if (xensiv_bgt60trxx_start_frame(&sensor.dev, true) != XENSIV_BGT60TRXX_STATUS_OK)
    {
        CY_ASSERT(0);
    }

    printf("BGT60TRXX setup complete\r\n");

    cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);

    memcpy(tx_buffer, &header, sizeof(header));

    for (;;) {
        // printf("reading radar..\r\n");
        // Wait for radar fifo data
        int result = 0;
        do {
            while (data_available == false);
            data_available = false;
            int result = xensiv_bgt60trxx_get_fifo_data(&sensor.dev, (uint16_t *)(tx_buffer + sizeof(struct hdr)), CHUNK_SIZE);

            // restart the radar if FIDO overflows.
            if (result == XENSIV_BGT60TRXX_STATUS_GSR0_ERROR) {
                printf("restarting radar..\r\n");
                xensiv_bgt60trxx_soft_reset(&sensor.dev, XENSIV_BGT60TRXX_RESET_FIFO);
                xensiv_bgt60trxx_start_frame(&sensor.dev, false);
                xensiv_bgt60trxx_start_frame(&sensor.dev, true);
                data_available = false;
            }

        } while (result != XENSIV_BGT60TRXX_STATUS_OK);

        // printf("waiting usb..\r\n");

        // |samples| is uint16_t array, we need to write as byte streams, therefore size
        // should be N_SAMPLES * 2
        // printf("writing... %hu %hu\r\n", ((uint16_t *)tx_buffer)[4], ((uint16_t *)tx_buffer)[4 + CHUNK_SIZE - 1]);
        USBD_CDC_Write(usb_cdcHandle, &tx_buffer, sizeof(tx_buffer), 0);
        USBD_CDC_WaitForTX(usb_cdcHandle, 0);
    }
}

/*********************************************************************
* Function Name: USBD_CDC_Echo_Init
**********************************************************************
* Summary:
*  Add communication device class to USB stack
*
* Parameters:
*  void
*
* Return:
*  void
**********************************************************************/

void usb_add_cdc(void) {

    static U8             OutBuffer[USB_FS_BULK_MAX_PACKET_SIZE];
    USB_CDC_INIT_DATA     InitData;
    USB_ADD_EP_INFO       EPBulkIn;
    USB_ADD_EP_INFO       EPBulkOut;
    USB_ADD_EP_INFO       EPIntIn;

    memset(&InitData, 0, sizeof(InitData));
    EPBulkIn.Flags          = 0;                             /* Flags not used */
    EPBulkIn.InDir          = USB_DIR_IN;                    /* IN direction (Device to Host) */
    EPBulkIn.Interval       = 0;                             /* Interval not used for Bulk endpoints */
    EPBulkIn.MaxPacketSize  = USB_FS_BULK_MAX_PACKET_SIZE;   /* Maximum packet size (64B for Bulk in full-speed) */
    EPBulkIn.TransferType   = USB_TRANSFER_TYPE_BULK;        /* Endpoint type - Bulk */
    InitData.EPIn  = USBD_AddEPEx(&EPBulkIn, NULL, 0);

    EPBulkOut.Flags         = 0;                             /* Flags not used */
    EPBulkOut.InDir         = USB_DIR_OUT;                   /* OUT direction (Host to Device) */
    EPBulkOut.Interval      = 0;                             /* Interval not used for Bulk endpoints */
    EPBulkOut.MaxPacketSize = USB_FS_BULK_MAX_PACKET_SIZE;   /* Maximum packet size (64B for Bulk in full-speed) */
    EPBulkOut.TransferType  = USB_TRANSFER_TYPE_BULK;        /* Endpoint type - Bulk */
    InitData.EPOut = USBD_AddEPEx(&EPBulkOut, OutBuffer, sizeof(OutBuffer));

    EPIntIn.Flags           = 0;                             /* Flags not used */
    EPIntIn.InDir           = USB_DIR_IN;                    /* IN direction (Device to Host) */
    EPIntIn.Interval        = 64;                            /* Interval of 8 ms (64 * 125us) */
    EPIntIn.MaxPacketSize   = USB_FS_INT_MAX_PACKET_SIZE ;   /* Maximum packet size (64 for Interrupt) */
    EPIntIn.TransferType    = USB_TRANSFER_TYPE_INT;         /* Endpoint type - Interrupt */
    InitData.EPInt = USBD_AddEPEx(&EPIntIn, NULL, 0);

    usb_cdcHandle = USBD_CDC_Add(&InitData);
}

/* [] END OF FILE */
