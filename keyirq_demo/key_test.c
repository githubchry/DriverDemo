#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define KEY_ENTER   28

struct key_event {
    int key;    //键值：W A S D ESC ENTER RESET....在input.h里面定义了所有的键值，也可以自定义
    int val;    //状态：1按下 0松开
};

int main()
{
    int fd = open("/dev/keyirq_demo", O_RDWR);
    if (fd < 0)
    {
        perror("open /dev/keyirq_demo");
        return -1;
    }
    
    struct key_event ev;
    while (1)
    {
        read(fd, &ev, sizeof(struct key_event));
        if (KEY_ENTER == ev.key)
        { 
            if (ev.val)
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
    close(fd);
    
    return 0;
}