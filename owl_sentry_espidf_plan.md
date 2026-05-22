# 猫头鹰哨兵 ESP-IDF BLE 通讯框架 + GitHub CI/CD 完整计划

## 一、目标概述

创建两个ESP-IDF项目(master/slave)，配置GitHub Actions使用ESP-IDF v5.1.2自动编译，输出两个merged.bin文件。

## 二、项目目录结构

```
RC_dual_servos/
├── .github/workflows/build-esp32c3.yml
├── master/CMakeLists.txt, sdkconfig.defaults, main/
└── slave/CMakeLists.txt, sdkconfig.defaults, main/
```

## 三、GitHub Actions工作流 (build-esp32c3.yml)

```yaml
name: Build ESP32C3 Projects
on:
  push:
    branches: [ main ]
  workflow_dispatch:

env:
  IDF_VERSION: v5.1.2
  TARGET_CHIP: esp32c3

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        project: [{name: master, path: ./master}, {name: slave, path: ./slave}]
    steps:
    - uses: actions/checkout@v4
    - uses: espressif/esp-idf-ci-action@v1
      with:
        esp_idf_version: v5.1.2
        target: esp32c3
        path: ${{ matrix.project.path }}
    - name: Build
      run: |
        . $IDF_PATH/export.sh
        cd ${{ matrix.project.path }}
        idf.py set-target esp32c3
        idf.py build
    - name: Merge BIN
      run: |
        cd ${{ matrix.project.path }}
        python $IDF_PATH/components/esptool_py/esptool/esptool.py \
          --chip esp32c3 merge_bin \
          -o build/${{ matrix.project.name }}_merged.bin \
          --flash_mode dio --flash_freq 80m --flash_size 4MB \
          0x0 build/bootloader/bootloader.bin \
          0x8000 build/partition_table/partition-table.bin \
          0x10000 build/${{ matrix.project.name }}.bin
    - uses: actions/upload-artifact@v4
      with:
        name: ${{ matrix.project.name }}_merged.bin
        path: ${{ matrix.project.path }}/build/${{ matrix.project.name }}_merged.bin
```

## 四、master项目文件

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(master)
```

**sdkconfig.defaults:**
```
CONFIG_IDF_TARGET="esp32c3"
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_BT_ENABLED=y
```

**main/CMakeLists.txt:**
```cmake
idf_component_register(SRCS "main.c" "ble_client.c"
                      INCLUDE_DIRS "."
                      PRIV_REQUIRES nvs_flash)
```

## 五、slave项目文件

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(slave)
```

**sdkconfig.defaults:**
```
CONFIG_IDF_TARGET="esp32c3"
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_BT_ENABLED=y
```

**main/CMakeLists.txt:**
```cmake
idf_component_register(SRCS "main.c" "ble_server.c"
                      INCLUDE_DIRS "."
                      PRIV_REQUIRES nvs_flash)
```

## 六、烧录命令

```bash
esptool.py --chip esp32c3 write_flash 0x0 master_merged.bin
esptool.py --chip esp32c3 write_flash 0x0 slave_merged.bin
```

## 七、输出文件

GitHub Actions编译完成后，在Artifacts中下载：
- master_merged.bin
- slave_merged.bin
