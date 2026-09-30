# esphome-hlk-ld2402-with-web-gui-tuning
ESPHome外部组件-海凌科LD2402雷达传感器，自带web-gui调参

A ESPhome External Component Working With HLK-LD2402 Radar Sensor. Tuning with build-in web gui

## 使用说明：
* 雷达固件版本需高于或等于v3.3.5,使用官方上位机连接到雷达uart来检查固件及ota固件
* 仅提供传感器类HA实体(距离、人在、固件版本、工作模式等)，所有调参实体集中到web-gui
* 提供http api端点，可使用外部http工具调用
* 没有自动门限生成功能
* 在esphome 2026.4.5上测试通过
* dev分支支持多实例，即配置多个ld2402组件。通过id进行区分，未测试，暂不合并
* 对内存与组件库有要求，只支持esp32系列并使用esp-idf框架，不支持esp8266
* 内存与看门狗特殊配置：
  ```
  esp32:
    variant: esp32c3
    framework:
      type: esp-idf
      sdkconfig_options:
        CONFIG_ESP_TASK_WDT_TIMEOUT_S: "30"
        CONFIG_ESP_INT_WDT_TIMEOUT_MS: "800"
      advanced:
        loop_task_stack_size: 10240
  ```
* 100% AI Coding

## 界面截图
* HA实体
  <img width="344" height="171" alt="image" src="https://github.com/user-attachments/assets/a772bc08-965b-43f1-9026-e1af5b18d4b0" />
  <img width="315" height="107" alt="image" src="https://github.com/user-attachments/assets/103855b2-38f1-4175-9c6e-4e58c3946b29" />
* web-gui
* 
   <img src="https://github.com/gasment/esphome_hlk_ld2402_with_web_gui_tuning/blob/main/web-gui.webp" />

## 配置示例
```
external_components:
  - source:
      type: git
      url: https://github.com/gasment/esphome_hlk_ld2402_with_web_gui_tuning
      ref: main
    components: [ld2402]
    refresh: always

uart:
  id: uart_ld2402
  tx_pin: GPIO6   #your uart tx pin
  rx_pin: GPIO7  #your uart rx pin
  baud_rate: 115200
  parity: NONE
  stop_bits: 1
  data_bits: 8  


ld2402:
  - id: ld2402_radar
    uart_id: uart_ld2402
    web_port: 8080  #set a web-gui port
    web_username: admin # set a web-gui basic-auth username
    web_password: admin  # set a web-gui basic-auth password

binary_sensor:
  - platform: ld2402
    ld2402_id: ld2402_radar
    name: "Occupancy"
    device_class: occupancy

sensor:
  - platform: ld2402
    ld2402_id: ld2402_radar
    distance:
      name: "Target Distance"
      filters:
          - throttle: 2s  
text_sensor:
  - platform: ld2402
    ld2402_id: ld2402_radar
    firmware_version:
      name: "LD2402 Firmware"
      icon: "mdi:chip"
      entity_category: "diagnostic"
    work_mode:
      name: "LD2402 Work Mode"
      icon: "mdi:cog-play"
      entity_category: "diagnostic"
```
## web调参
* 访问设备IP:web_port,进入调参界面
* 实时能量需要先开启工程模式，调试完记得关闭工程模式
* 任何修改都记得保存到flash来持久化
  
## http api
| 端点 | 方法 | 用途 |
|---|---|---|
| `/` | GET | Web 管理页面（HTML） |
| `/api/info` | GET | 获取雷达状态/配置信息 |
| `/api/cmd` | POST | 发送控制命令 |
| `/sse` | GET | 实时能量数据流|

* cmd端点payload格式，可通过web调试或分析源码ld2402.cpp内的handle_api_cmd方法取得


## 多实例
```
ld2402:
  - id: ld2402_radar_1
    uart_id: uart_ld2402_1
    web_port: ${web_port_1}
    web_username: ${web_user}
    web_password: ${web_passwd}

  - id: ld2402_radar_2
    uart_id: uart_ld2402_2
    web_port: ${web_port_2}   #需要使用不同的端口，多实例间的端口最好不要相邻
    web_username: ${web_user}
    web_password: ${web_passwd}
```
