/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_partition.h"
#include "esp_crc.h"
#include "esp_secure_cert_read.h"
#include "esp_secure_cert_tlv_config.h"

#if __has_include("esp_idf_version.h")
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "spi_flash_mmap.h"
#endif
#endif

static const char *TAG = "esp_secure_cert_tlv";

#define MIN_ALIGNMENT_REQUIRED 16
/* This is the mininum required flash address alignment in bytes to write to an encrypted flash partition */

/*
 * Map the entire esp_secure_cert partition
 * and return the virtual address.
 *
 * @note
 * The mapping is done only once and function shall
 * simply return same address in case of successive calls.
 **/
static const void *esp_secure_cert_get_mapped_addr(void)
{
    // Once initialized, these variable shall contain valid data till reboot.
    static const void *esp_secure_cert_mapped_addr;
    if (esp_secure_cert_mapped_addr) {
        return esp_secure_cert_mapped_addr;
    }

    esp_partition_iterator_t it = esp_partition_find(ESP_SECURE_CERT_TLV_PARTITION_TYPE,
                                  ESP_PARTITION_SUBTYPE_ANY, ESP_SECURE_CERT_TLV_PARTITION_NAME);
    if (it == NULL) {
        ESP_LOGE(TAG, "Partition not found.");
        return NULL;
    }

    const esp_partition_t *partition = esp_partition_get(it);
    if (partition == NULL) {
        ESP_LOGE(TAG, "Could not get partition.");
        return NULL;
    }

    /* Encrypted partitions need to be read via a cache mapping */
    spi_flash_mmap_handle_t handle;
    esp_err_t err;

    /* Map the entire partition */
    err = esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA, &esp_secure_cert_mapped_addr, &handle);
    if (err != ESP_OK) {
        return NULL;
    }
    return esp_secure_cert_mapped_addr;
}

/*
 * Find the offset of tlv structure of given type in the esp_secure_cert partition
 *
 * Note: This API also validates the crc of the respective tlv before returning the offset
 * @input
 * esp_secure_cert_addr     Memory mapped address of the esp_secure_cert partition
 * type                     Type of the tlv structure.
 *                          for calculating current crc for esp_secure_cert
 *
 * tlv_address              Void pointer to store tlv address
 *
 */
static esp_err_t esp_secure_cert_find_tlv(const void *esp_secure_cert_addr, esp_secure_cert_tlv_type_t type, void **tlv_address)
{
    /* start from the begining of the partition */
    uint16_t tlv_offset = 0;
    while (1) {
        esp_secure_cert_tlv_header_t *tlv_header = (esp_secure_cert_tlv_header_t *)(esp_secure_cert_addr + tlv_offset);
        ESP_LOGD(TAG, "Reading from offset of %d from base of esp_secure_cert", tlv_offset);
        if (tlv_header->magic != ESP_SECURE_CERT_TLV_MAGIC) {
            if (type == ESP_SECURE_CERT_TLV_END) {
                /* The invalid magic means last tlv read successfully was the last tlv structure present,
                 * so send the end address of the tlv.
                 * This address can be used to add a new tlv structure. */
                *tlv_address = (void *) tlv_header;
                return ESP_OK;
            }
            ESP_LOGD(TAG, "Unable to find tlv of type: %d", type);
            ESP_LOGE(TAG, "Expected magic byte is %04X, obtained magic byte = %04X", ESP_SECURE_CERT_TLV_MAGIC, (unsigned int) tlv_header->magic);
            return ESP_FAIL;
        }
        uint8_t padding_length = MIN_ALIGNMENT_REQUIRED - (tlv_header->length % MIN_ALIGNMENT_REQUIRED);
        padding_length = (padding_length == MIN_ALIGNMENT_REQUIRED) ? 0 : padding_length;
        // crc_data_len = header_len + data_len + padding
        size_t crc_data_len = sizeof(esp_secure_cert_tlv_header_t) + tlv_header->length + padding_length;
        if (((esp_secure_cert_tlv_type_t)tlv_header->type) == type) {
            *tlv_address = (void *) tlv_header;
            uint32_t data_crc = esp_crc32_le(UINT32_MAX, (const uint8_t * )tlv_header, crc_data_len);
            esp_secure_cert_tlv_footer_t *tlv_footer = (esp_secure_cert_tlv_footer_t *)(esp_secure_cert_addr + crc_data_len + tlv_offset);
            if (tlv_footer->crc != data_crc) {
                ESP_LOGE(TAG, "Calculated crc = %04X does not match with crc"
                         "read from esp_secure_cert partition = %04X", (unsigned int)data_crc, (unsigned int)tlv_footer->crc);
                return ESP_FAIL;
            }
            ESP_LOGD(TAG, "tlv structure of type %d found and verified", type);
            return ESP_OK;
        } else {
            tlv_offset = tlv_offset + crc_data_len + sizeof(esp_secure_cert_tlv_footer_t);
        }
    }
}

esp_err_t esp_secure_cert_tlv_get_addr(esp_secure_cert_tlv_type_t type, char **buffer, uint32_t *len)
{
    esp_err_t err;
    char *esp_secure_cert_addr = (char *)esp_secure_cert_get_mapped_addr();
    if (esp_secure_cert_addr == NULL) {
        ESP_LOGE(TAG, "Error in obtaining esp_secure_cert memory mapped address");
        return ESP_FAIL;
    }
    void *tlv_address;
    err = esp_secure_cert_find_tlv(esp_secure_cert_addr, type, &tlv_address);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not find the tlv of type %d", type);
        return err;
    }
    esp_secure_cert_tlv_header_t *tlv_header = (esp_secure_cert_tlv_header_t *) tlv_address;
    *buffer = (char *)&tlv_header->value;
    *len = tlv_header->length;
    return ESP_OK;
}

#ifdef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
esp_ds_data_ctx_t *esp_secure_cert_tlv_get_ds_ctx(void)
{
    esp_err_t esp_ret;
    esp_ds_data_ctx_t *ds_data_ctx;

    ds_data_ctx = (esp_ds_data_ctx_t *)calloc(1, sizeof(esp_ds_data_ctx_t));
    if (ds_data_ctx == NULL) {
        ESP_LOGE(TAG, "Error in allocating memory for esp_ds_data_context");
        goto exit;
    }

    uint32_t len;
    esp_ds_data_t *esp_ds_data;
    esp_ret = esp_secure_cert_tlv_get_addr(ESP_SECURE_CERT_DS_DATA_TLV, (void *) &esp_ds_data, &len);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in reading ds_data, returned %04X", esp_ret);
        goto exit;
    }

    esp_ds_data_ctx_t *ds_data_ctx_flash;
    esp_ret = esp_secure_cert_tlv_get_addr(ESP_SECURE_CERT_DS_CONTEXT_TLV, (void *) &ds_data_ctx_flash, &len);
    memcpy(ds_data_ctx, ds_data_ctx_flash, len);
    ds_data_ctx->esp_ds_data = esp_ds_data;
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in reading ds_context, returned %04X", esp_ret);
        goto exit;
    }
    return ds_data_ctx;
exit:
    free(ds_data_ctx);
    return NULL;
}

void esp_secure_cert_tlv_free_ds_ctx(esp_ds_data_ctx_t *ds_ctx)
{
    free(ds_ctx);
}
#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */

bool esp_secure_cert_is_tlv_partition(void)
{
    char *esp_secure_cert_addr = (char *)esp_secure_cert_get_mapped_addr();
    if (esp_secure_cert_addr == NULL) {
        return 0;
    }
    esp_secure_cert_tlv_header_t *tlv_header = (esp_secure_cert_tlv_header_t *)(esp_secure_cert_addr );
    if (tlv_header->magic == ESP_SECURE_CERT_TLV_MAGIC) {
        ESP_LOGI(TAG, "TLV partition identified");
        return 1;
    }
    return 0;
}

#ifndef CONFIG_ESP_SECURE_CERT_SUPPORT_LEGACY_FORMATS
esp_err_t esp_secure_cert_get_device_cert(char **buffer, uint32_t *len)
{
    return esp_secure_cert_tlv_get_addr(ESP_SECURE_CERT_DEV_CERT_TLV, buffer, len);
}

esp_err_t esp_secure_cert_free_device_cert(char *buffer)
{
    (void) buffer; /* nothing to do */
    return ESP_OK;
}

esp_err_t esp_secure_cert_get_ca_cert(char **buffer, uint32_t *len)
{
    return esp_secure_cert_tlv_get_addr(ESP_SECURE_CERT_CA_CERT_TLV, buffer, len);
}

esp_err_t esp_secure_cert_free_ca_cert(char *buffer)
{
    (void) buffer; /* nothing to do */
    return ESP_OK;
}

#ifndef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
esp_err_t esp_secure_cert_get_priv_key(char **buffer, uint32_t *len)
{
    return esp_secure_cert_tlv_get_addr(ESP_SECURE_CERT_PRIV_KEY_TLV, buffer, len);
}

esp_err_t esp_secure_cert_free_priv_key(char *buffer)
{
    (void) buffer; /* nothing to do */
    return ESP_OK;
}

#else /* !CONFIG_ESP_SECURE_CERT_DS_PEIPHERAL */

esp_ds_data_ctx_t *esp_secure_cert_get_ds_ctx(void)
{
    return esp_secure_cert_tlv_get_ds_ctx();
}

void esp_secure_cert_free_ds_ctx(esp_ds_data_ctx_t *ds_ctx)
{
    esp_secure_cert_tlv_free_ds_ctx(ds_ctx);
}
#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */
#endif /* CONFIG_ESP_SECURE_CERT_SUPPORT_LEGACY_FORMATS */
