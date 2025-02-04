# This file contains the list of changes across different versions

## v2.0.5
* Fixed targets in Kconfig to reflect DS Peripheral compatibility

## v2.0.4
* Add implementation of `esp_secure_cert_free_*` APIs for TLV configuration.

## v2.0.3
* Added C linkage so that C++ code can find the definitions for secure cert APIs.
* Minor documentation fixes.

## v2.0.2
* Updated reference to the new esp_partition component (IDFv5.0)

## v2.0.1
* Added fixes for build failures with `-Wstrict-prototypes` CFLAG.
* Added fix for build failure with toolchain change in IDFv4.x and IDFv5.x

## v2.0.0
* Added esp-secure-cert-tool to PyPi.
* Restructure esp-secure-cert-tool
### Breaking changes in v2.0.0
* Added the support for TLV format for storing data in esp_secure_cert partition.
* Make the TLV `cust_flash_tlv` as the default flash format.
* Marked all the supported flash formats before TLV as legacy: `cust_flash`, `nvs`.
* esp_secure_cert_app: Updated the partition table for the example

## v1.0.3
* esp_secure_cert API now Dynamically identify the type of partitionand access the data accordingly
* esp_secure_cert_app: Enable support for target esp32
* Added tests based on qemu
* Added priv_key functionality to the configure_esp_secure_cert.py script.
### Breaking changes in v1.0.3
* Removed all the configuration options related to selecting the type of `esp_secure_cert` partition
* Remove `esp_secure_cert_get_*_addr` API, the contents can now be obtained through `esp_secure_cert_get_*` API.
* Remove APIs to obain the contents of the DS contexts e.g. efuse key id, ciphertext, iv etc. The contents can be accesed from inside the DS context which can be obtained through respective API.
* Breaking change in the `esp_secure_cert_get_*` API:
The API now accepts `char **buffer` instead of `char *buffer`. It will allocate the required memory dynamically and directly if necessary and provide the respective pointer.
