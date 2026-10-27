# esp32-spi-hal

The ESP32 side of [ext-spi](package:php-baremetal/ext-spi): it implements the neutral SPI HAL (`bm_spi_hal.h`)
over ESP-IDF's `spi_master` driver. It opens SPI hosts, attaches devices and runs the transfers behind
`Baremetal\Spi\Bus` and `Baremetal\Spi\Device`.

| Field | Value |
| --- | --- |
| Type | `bm-hal` |
| Platform | `esp32` (`extra.baremetal.platform`) |
| Provides | `php-baremetal/spi-hal` |
| ESP-IDF components | `esp_driver_spi` (`idf-requires`) |

## Install

Projects do not require it. A project requires ext-spi, which requires the virtual `php-baremetal/spi-hal`;
conductor reads the platform from `extra.baremetal.sdk` (`esp32@^2.0`) and adds this package while Composer
resolves:

```bash
composer require php-baremetal/ext-spi
```

A driver that is ESP32-only, such as [esp32-display-st77916](package:php-baremetal/esp32-display-st77916),
requires this package directly.

## Behaviour

- **Host setup.** `$host` of `Bus` is used as the `spi_host_device_t` (`1` is `SPI2_HOST`, `2` is `SPI3_HOST`).
  The host is initialised with DMA on an automatically picked channel and a maximum transfer of 32768 bytes.
  When `data2` and `data3` are given they become the quad WP and HD lines, for a four-line bus.
- **Devices.** Each raw device is attached with its clock, mode and CS pin and a queue of 4 transactions. Its
  transaction descriptor lives in internal RAM, because `spi_master` holds a pointer to it while the transfer
  runs and the PHP task's stack is in PSRAM.
- **Transfers.** One `spi_device_transmit()` per call: it blocks the calling PHP task until the transfer is
  done. Buffers outside DMA-capable memory (PHP strings, the stack) are copied through a bounce buffer by
  `spi_master`. A zero-length call does nothing; more than 32768 bytes returns `ESP_ERR_INVALID_SIZE`.
- **Error names.** Exceptions end with `esp_err_to_name()` of the error, or with one of the php-baremetal
  names: `chip select already used by another driver on this bus` and
  `the host or a pin is already used by an SPI bus open with other pins`.
- **Hosts the firmware uses.** The SDK's microSD storage in SPI mode runs on `SPI3_HOST` and the W5500
  network on `SPI2_HOST`. Opening an already initialised host fails with `ESP_ERR_INVALID_STATE`.

Buses and devices are never closed: ext-spi keeps them for the whole run.

## License

MIT.
