#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
/*
root@firefly:/mnt# ls /dev/input/
by-path  event0  event1  event2  event3
root@firefly:/mnt# insmod simple_input_drv.ko 
root@firefly:/mnt# ls /dev/input/
by-path  event0  event1  event2  event3  event4
root@firefly:/mnt# 
caozuo
通过以上操作可以确定DEV_PATH是/dev/input/event4
*/
#define DEV_PATH   "/dev/input/event4"

/*
struct input_event {
	struct timeval time;    // 时间戳
	__u16 type;             // 数据类型
	__u16 code;             // 具体的数据是什么
	__s32 value;            // 值是什么
};
*/

int main()
{
    int ret = -1;
    int fd = -1;
    struct input_event ev;
    
    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0)
    {
        perror("open" DEV_PATH);
        return -1;
    }
    

    while (1)
    {
        ret = read(fd, &ev, sizeof(struct input_event));
        if (ret < 0)
        {
            perror("read ");
            return -1;
        }

        if (EV_KEY == ev.type)
        {
            if (KEY_POWER == ev.code)
            {
                if (ev.value)
                {
                    //按下
                    printf("%s,%s:%d key down\n", __FILE__, __func__, __LINE__);
                }
                else
                {
                    //松开
                    printf("%s,%s:%d key up\n", __FILE__, __func__, __LINE__);
                }
            }
        }
        
    }
    close(fd);
    
    return 0;
}