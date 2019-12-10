
虽然linux内核已经自带了一套通用的GPIO驱动，可以在用户态配置GPIO的接口，使用export方式控制GPIO的输入输出，但是这种方法存在局限性：  
1.在GPIO已经被占用的情况下无法被控制  
2.每次只能控制一个GPIO引脚  
  
使用映射虚拟寄存器方式驱动gpio可以解决以上问题。  
    
参考：   
[Linux内核中ioremap映射的透彻理解](https://blog.csdn.net/do2jiang/article/details/5450839)  
[实验探究 ioremap](https://blog.csdn.net/lizuobin2/article/details/52046369)   
[ioremap() 函数解析](https://blog.csdn.net/zqixiao_09/article/details/50859505)   
  
# CPU对IO端口的编址方式  
几乎每一种外设都是通过读写设备上的寄存器来进行的，通常包括控制寄存器、状态寄存器和数据寄存器三大类，外设的寄存器通常被连续地编址。根据CPU体系结构的不同，CPU对IO端口的编址方式有两种：  
- I/O 映射方式（I/O-mapped）  
典型地，如X86处理器为外设专门实现了一个单独的地址空间，称为"**I/O地址空间**"或者"**I/O端口空间**"，CPU通过专门的I/O指令（如X86的IN和OUT指令）来访问这一空间中的地址单元。ps：arm体系架构是io内存。  
  
- 内存映射方式（Memory-mapped）  
　 RISC指令系统的CPU（如ARM、PowerPC等）通常只实现一个物理地址空间，**外设I/O端口成为内存的一部分**。此时，CPU可以象访问一个内存单元那样访问外设I/O端口，而不需要设立专门的外设I/O指令。  
  
**但是，这两者在硬件实现上的差异对于软件来说是完全透明的，驱动程序开发人员可以将内存映射方式的I/O端口和外设内存统一看作是"I/O内存"资源。**  
  
一般来说，在系统运行时，外设的I/O内存资源的物理地址是已知的，由硬件的设计决定。但是CPU通常并没有为这些已知的外设I/O内存资源的物理地址预定义虚拟地址范围，驱动程序并不能直接通过物理地址访问I/O内存资源，而必须将它们映射到核心虚地址空间内（通过页表），然后才能根据映射所得到的核心虚地址范围，通过访内指令访问这些I/O内存资源。  
  
  
# ioremap函数解析  
    
 ioremap宏定义在asm/io.h内，用来将I/O内存资源的物理地址映射到核心虚地址空间（3GB－4GB，这里是内核空间）中如下：  
```  
#define ioremap(cookie,size)           __ioremap(cookie,size,0)  
```  
__ioremap函数原型为(arm/mm/ioremap.c)：  
```  
void __iomem * __ioremap(unsigned long phys_addr, size_t size, unsigned long flags);  
```  
参数：  
phys_addr：要映射的起始的IO地址  
size：要映射的空间的大小  
flags：要映射的IO空间和权限有关的标志  
  
该函数返回映射后的内核虚拟地址(3G-4G). 接着便可以通过读写该返回的内核虚拟地址去访问之这段I/O内存资源。  
  
# iounmap函数解析  
iounmap函数用于取消ioremap()所做的映射，原型如下：  
```  
void iounmap(void * addr);  
```  
  

这两个函数都是实现在mm/ioremap.c文件中。

# 映射虚拟寄存器方式驱动rk3288 gpio  
见源码