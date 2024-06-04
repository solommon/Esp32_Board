#### 介绍
	欢迎大家使用本ESP32仓库源码作为学习的途径。仓库下载方式:
	git clone --recursive https://gitee.com/vi-iot/esp32-board.git

	本仓库所有源码都是依赖esp-idf库进行开发的，esp-idf版本为v5.2。
        所有工程都经过本人亲自调试OK，大家可以参考学习，或者直接用在自己的项目中。
        例程配套的开发板链接如下：
	https://item.taobao.com/item.htm?ft=t&id=802401650392&spm=a21dvs.23580594.0.0.4fee645eXpkfcp&skuId=5635015963649
	希望大家多多支持

#### esp-idf说明
	esp-idf是官方推荐使用的开发方式，以后大家实际工作中基本都会采用这种开发方式，
        乐鑫推荐在ubuntu下对源码进行编译，在ubuntu下编译速度很快，
        ubuntu下整个esp-idf加上自己的工程，编译在1-2分钟左右，
        而使用windows环境编译，大概是10分钟，而且ubuntu环境下开发兼容性好，

        本仓库例程配套的教程链接如下：
	链接：https://pan.baidu.com/s/1kCjD8yktZECSGmHomx_veg?pwd=q8er 
	提取码：q8er 

	教程中包含了如何搭建开发环境、freeRTOS的使用、基础外设开发、存储相关、WIFI等教学，
        完全适合各阶段人群

#### 仓库架构
	本仓库包含多个例程，均可单独编译，每个工程说明如下：
	
	aliot		————展示如何连接阿里云IOT平台
	ap_prov		————ap配网例程
	button		————按键操作例程
	dht11		————DHT11温湿度传感器数据读取
	display		————LVGL移植和st7789LCD驱动芯片代码
	helloworld	————基础程序，打印helloworld
	ledc		————简单的点灯程序和呼吸灯程序
	mqtt		————mqtt连接例程
	ntc			————使用ADC读取热敏电阻试验
	nvs			————NVS操作
	onenet		————展示如何连接中移onenet平台
	partition	————ESP32的分区操作
	rtos		————freeRTOS常用的示例
	sdcard		————读写MicroSD卡
	smartconfig	————一键配网例程
	softap		————ESP32 AP工作模式例程
	spiffs		————spiffs文件系统例程
	sr04		————读取SR04超声波传感器数据
	sr602		————读取SR602热释电红外人体传感器数据
	weather		————根据ip地址获取心知天气
	web_monitor ————小项目，ESP32环境监控
	wifi		————STA模式
	ws2812		————点亮WS2812例程

	源码配套教程和资料：
	链接：https://pan.baidu.com/s/1kCjD8yktZECSGmHomx_veg?pwd=q8er 
	提取码：q8er 

