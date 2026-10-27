/*
   +----------------------------------------------------------------------+
   | php-baremetal: esp32-spi-hal                                         |
   +----------------------------------------------------------------------+
   | Copyright (c) 2026 Gianfrancesco Aurecchia                           |
   +----------------------------------------------------------------------+
   | This source file is subject to the MIT license, that is bundled with |
   | this package in the file LICENSE.                                    |
   +----------------------------------------------------------------------+
   | Authors: Gianfrancesco Aurecchia <gianfri@php-baremetal.com>         |
   +----------------------------------------------------------------------+
*/

/* ESP32 implementation of the spi HAL (bm_spi_hal.h) over esp_driver_spi's spi_master. */
#include "bm_spi_hal.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include <string.h>

#define BUS_MAX_XFER  32768
#define DEV_QUEUE     4

/* Per-device HAL state, in internal RAM: spi_master keeps a pointer to the transaction while it runs,
 * so it must not sit on the PSRAM PHP task stack. One transmit at a time per device. */
typedef struct {
    spi_device_handle_t handle;
    spi_transaction_t trans;
} hal_dev_t;

bm_err_t bm_spi_hal_bus_open(spi_bus_t *bus)
{
    spi_bus_config_t cfg = {
        .mosi_io_num = bus->mosi,
        .miso_io_num = bus->miso,
        .sclk_io_num = bus->sclk,
        .quadwp_io_num = bus->quad ? bus->data2 : -1,
        .quadhd_io_num = bus->quad ? bus->data3 : -1,
        .max_transfer_sz = BUS_MAX_XFER,
    };
    return spi_bus_initialize((spi_host_device_t) bus->host, &cfg, SPI_DMA_CH_AUTO);
}

void bm_spi_hal_bus_close(spi_bus_t *bus)
{
    spi_bus_free((spi_host_device_t) bus->host);
}

bm_err_t bm_spi_hal_dev_open(spi_dev_t *d)
{
    spi_device_interface_config_t dc = {
        .clock_speed_hz = (int) d->hz,
        .mode = d->mode,
        .spics_io_num = d->cs,
        .queue_size = DEV_QUEUE,
    };
    hal_dev_t *h = heap_caps_calloc(1, sizeof(*h), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!h) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t e = spi_bus_add_device((spi_host_device_t) d->bus->host, &dc, &h->handle);
    if (e != ESP_OK) {
        heap_caps_free(h);
        return e;
    }
    d->hal = h;
    return BM_OK;
}

void bm_spi_hal_dev_close(spi_dev_t *d)
{
    if (d->hal) {
        hal_dev_t *h = d->hal;
        spi_bus_remove_device(h->handle);
        heap_caps_free(h);
        d->hal = NULL;
    }
}

/* tx/rx outside DMA-capable memory (PSRAM strings, stack) are bounced by spi_master. */
bm_err_t bm_spi_hal_transfer(spi_dev_t *d, const uint8_t *tx, uint8_t *rx, size_t n)
{
    if (n == 0) {
        return BM_OK;
    }
    if (n > BUS_MAX_XFER) {
        return ESP_ERR_INVALID_SIZE;
    }
    hal_dev_t *h = d->hal;
    memset(&h->trans, 0, sizeof(h->trans));
    h->trans.length = n * 8;
    h->trans.tx_buffer = tx;
    h->trans.rx_buffer = rx;
    return spi_device_transmit(h->handle, &h->trans);
}

const char *bm_spi_hal_err_name(bm_err_t err)
{
    if (err == BM_ERR_CS_IN_USE) {
        return "chip select already used by another driver on this bus";
    }
    if (err == BM_ERR_BUS_PINS) {
        return "the host or a pin is already used by an SPI bus open with other pins";
    }
    return esp_err_to_name(err);
}
