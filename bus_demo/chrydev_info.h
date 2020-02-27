#pragma once

//定义结构体 存放设备信息
struct chrydev_platform_data {
    char *name;         
    int irqno;          //中断号
    uint32_t phyaddr; //寄存器地址
};
