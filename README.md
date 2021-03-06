# hardware-thermal #

This module contains the STMicroelectronics Thermal hardware service source code.
It is part of the STMicroelectronics delivery for Android (see the [delivery][] for more information).

[delivery]: https://wiki.st.com/stm32mpu/wiki/STM32MP15_distribution_for_Android_release_note_-_v2.0.0

## Description ##

This module version is the updated version for STM32MP15 distribution for Android V2.0
Please see the Android delivery release notes for more details.

It is based on the Module Thermal API version 2.0.

## Documentation ##

* The [release notes][] provide information on the release.
* The [distribution package][] provides detailed information on how to use this delivery.

[release notes]: https://wiki.st.com/stm32mpu/wiki/STM32MP15_distribution_for_Android_release_note_-_v2.0.0
[distribution package]: https://wiki.st.com/stm32mpu/wiki/STM32MP1_Distribution_Package_for_Android

## Dependencies ##

This module can't be used alone. It is part of the STMicroelectronics delivery for Android.
To be able to use it the device.mk must have the following packages:
```
PRODUCT_PACKAGES += \
    android.hardware.thermal@2.0-service.stm32mp1
```

## Containing ##

This directory contains the sources and associated Android makefile to generate the android.hardware.thermal@2.0-service.stm32mp1 binary.

## License ##

This module is distributed under the Apache License, Version 2.0 found in the [LICENSE](./LICENSE) file.
