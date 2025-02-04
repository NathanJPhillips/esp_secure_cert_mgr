# ESP Secure Certificate Manager

The *esp_secure_cert_mgr* is a simplified interface to access the PKI credentials of a device pre-provisioned with the
Espressif Provisioning Service. It provides the set of APIs that are required to access the contents of
the `esp_secure_cert` partition.
A demo example has also been provided with the `esp_secure_cert_mgr`, more details can be found out
in the [example README](https://github.com/espressif/esp_secure_cert_mgr/blob/main/examples/esp_secure_cert_app/README.md)

## How to use the `esp_secure_cert_mgr` in your project ?
---
There are two ways to use `esp_secure_cert_mgr` in your project:

1) Add `esp_secure_cert_mgr` to your project with help of IDF component manager:
* The component is hosted at https://components.espressif.com/component/espressif/esp_secure_cert_mgr. Please use the same link to obtain the latest available version of the component along with the instructions on how to add it to your project.
* Additional details about using a component through IDF component manager can be found [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html#using-with-a-project)


2) Add `esp_secure_cert_mgr` as an extra component in your project.

* Download `esp_secure_cert_mgr` with:
```
    git clone https://github.com/espressif/esp_secure_cert_mgr.git
```
* Include  `esp_secure_cert_mgr` in `ESP-IDF` with setting `EXTRA_COMPONENT_DIRS` in CMakeLists.txt/Makefile of your project.For reference see [Optional Project Variables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#optional-project-variables)

## What is Pre-Provisioning?

With the Espressif Pre-Provisioning Service, the ESP modules are pre-provisioned with an encrypted RSA private key and respective X509 public certificate before they are shipped out to you. The PKI credentials can then be registered with the cloud service to establish a secure TLS channel for communication. With the pre-provisioning taking place in the factory, it provides a hassle-free PKI infrastructure to the Makers. You may use this repository to set up your test modules to validate that your firmware works with the pre-provisioned modules that you ordered through Espressif's pre-provisioning service.

## `esp_secure_cert` partition
When a device is pre-provisioned that means the PKI credentials are generated for the device. The PKI credentials are then stored in a partition named
*esp_secure_cert*.

The `esp_secure_cert` partition can be generated on host with help of [configure_esp_secure_cert.py](https://github.com/espressif/esp_secure_cert_mgr/blob/main/tools/configure_esp_secure_cert.py) utility, more details about the utility can be found in the [tools/README](https://github.com/espressif/esp_secure_cert_mgr/tree/main/tools#readme).

For esp devices that support DS peripheral, the pre-provisioning is done by leveraging the security benefit of the DS peripheral. In that case, all of the data which is present in the *esp_secure_cert* partition is completely secure.

When the device is pre-provisioned with help of the DS peripheral then by default the partition primarily contains the following data:
1) Device certificate: It is the public key/ certificate for the device's private key. It is used in TLS authentication.
2) CA certificate: This is the certificate of the CA which is used to sign the device cert.
3) Ciphertext: This is the encrypted private key of the device. The ciphertext is encrypted using the DS peripheral, thus it is completely safe to store on the flash.

As listed above, the data only contains the public certificates and the encrypted private key and hence it is completely secure in itself. There is no need to further encrypt this data with any additional security algorithm.

### `esp_secure_cert` partition format:
The data is stored in the TLV format in the `esp_secure_cert`.
The data is stored in the following manner:

`TLV_Header -> data -> TLV_Footer`

* TLV header: It contains the information regarding the data such as the type of the data and the length of the data.
* TLV footer: It contains the crc of the data along with the header.

For more details about the TLV, please take a look at [tlv_config.h](https://github.com/espressif/esp_secure_cert_mgr/tree/main/private_include/esp_secure_cert_tlv_config.h).

* For TLV format the `partitions.csv` file for the project should contain the following line which enables it to identify the `esp_secure_cert` partition:

```
# Name, Type, SubType, Offset, Size, Flags
esp_secure_cert, 0x3F, , 0xD000, 0x2000,
```

Please note that, TLV format uses compact data representation and hence partition size is kept as 8KiB.

> Note: The TLV read API expects that a padding of appropriate size is added to data to make it size as a multiple of 16 bytes, the partition generation utility i.e. [configure_esp_secure_cert.py](https://github.com/espressif/esp_secure_cert_mgr/blob/main/tools/configure_esp_secure_cert.py) takes care of this internally while generating the partition. 


### Legacy formats for `esp_secure_cert` partition:
`esp_secure_cert` partition also supports two legacy flash formats.
The support for these can be enabled through following menuconfig option:
* `Component config > ESP Secure Cert Manager -> Enable support for legacy formats`

1) *cust_flash*: In this case, the partition is a custom flash partition. The data is directly stored over the flash.
* In this case the `partitions.csv` file for the project should contain the following line which enables it to identify the `esp_secure_cert` partition.

```
# Name, Type, SubType, Offset, Size, Flags
esp_secure_cert, 0x3F, , 0xD000, 0x6000,
```

2) *nvs partition*: In this case, the partition is of the `nvs` type. The `nvs_flash` abstraction layer from the ESP-IDF is used to store and then retreive the contents of the `esp_secure_cert` partition.

* In this case the `partitions.csv` file for the project should contain the following line which enables it to identify the `esp_secure_cert` partition.

```
# Name, Type, SubType, Offset, Size, Flags
esp_secure_cert, data, nvs, 0xD000, 0x6000,
```
