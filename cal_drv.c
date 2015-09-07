/*
 *  Copyright (c) 2012.  	Computer Architecture and System Laboratory, CASLab
 *							Dept. of Electrical Engineering & Inst. of Computer and Communication Engineering
 *							National Cheng Kung University, Tainan, Taiwan
 *  All Right Reserved
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
need to care 4 functions
-----------1. cal_write() ,we need to fix
-----------2. cal_read() ,we need to fix
-----------3. cal_unlocked_ioctl(),we don't need to fix
-----------4. cal_interrupt(),we don't need to fix

*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/page.h>

#include <linux/device.h>
#include <asm/irq.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irqflags.h>
#include <linux/wait.h>


/* hardware device setting value  */
//#define DRIVER_NAME "cal_hw"
//#define DEVICE_NAME "calculator"
#define DRIVER_NAME "lena_hw"
#define DEVICE_NAME "lenadriver"
#define CAL_MAJOR		254
#define CAL_MINOR		0
#define IRQ_VIC_BRIDGE	30
#define PIC_BASE		32
#define CALCULATOR_BASE		0x80000000

/*  command */
#define OP_SET	1
#define GO		5

/*  address offset  */
#define operand_offset 		0x4
#define result_offset 		0x10
//#define operator_offset		0x4

MODULE_AUTHOR("KUAN-CHUNG CHEN");
MODULE_LICENSE("GPL");

static DECLARE_WAIT_QUEUE_HEAD(wq);
static int flag = 0;
static int count = 0;
static int cal_major = CAL_MAJOR;
static int cal_minor = CAL_MINOR;

static int buffer[100];
void __iomem *base;


static int cal_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int cal_release(struct inode *inode, struct file *file)
{
	return 0;
}
//這裡最重要的兩個function為：copy_to_user及copy_from_user。
//因為關係到將data從user space轉換到kernel space，因此我們需要這兩隻函式幫我們把一整串data轉移到driver，driver在針對這整筆資料做相對應的動作。
//從下兩張圖看到，device_read透過readl將值讀回，並透過copy_to_ user送回應用程式(注意不是用return)，而device_write則以copy_ from_user得到應用程式想寫入device的值，
//再以writel寫進device。

static ssize_t cal_read(struct file *filp, char *buff, size_t len, loff_t *off)
{
	int i;

	/* Only result is readable memory address */
	if(len>4)
		len = 4;

	for( i = 0 ; i < len/4; i++)
		buffer[i] = readl(base + result_offset + i*4);

	i=copy_to_user(buff, buffer, len);

	return len;
}

static ssize_t cal_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	int i;

	/* Two operands are writeable memory address */
	if (len > 12)
		len = 12;

	i=copy_from_user(buffer, buff, len);

	for(i = 0; i < len/4; i++) {
		writel(buffer[i], base + operand_offset +(i*4));   //以writel寫進device
	}

	return len;
}

static long cal_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)  
{			//裡面較特殊的是unlocked_ioctl，其被用來對特定register位址寫入特定值，因此從R/W function中獨立出來。

	switch(cmd)
	{
	case OP_SET:  //此條件在專案中並沒有用到
		//writel((unsigned int)arg, base + operator_offset);
		break;
	case GO:		
			//這邊我們還結合了waiting queue的概念，因為當應用程式使用“GO”指令要求calculator進行運算時，
			//我們預設應用程式應該是處在等待運算結果回來的狀態，因此將此程序塞入waiting queue，讓出CPU給其他程序得以進行。
			//我們希望他先進入等待佇列而不是採用busy waiting。
			//在ISR與IOCTL中我們都提到了waiting queue的概念，也就是當應用程式必須等待硬體回應時，我們希望他先進入等待佇列而不是採用busy waiting。
			//Linux Kernel本身就有提供此功能,標頭檔為 <linux/wait.h>
			//首先必須先宣告等待佇列以及喚醒條件旗標
			//static DECLARE_WAIT_QUEUE_HEAD(wq);
			//static int flag = 0;
			//當我們需要應用程式進去等待佇列時，採用
			//wait_event_interruptible(wq, flag!=0);
			//則應用程式會進入wq等待被喚醒，且必要條件為flag!=0時。
			//而要喚醒程式時，
			//會先設定喚醒條件旗標: flag = 1;
			//在將程序從wq喚醒: wake_up_interruptible(&wq);
		writel((unsigned int)arg, base);  //指傳base,0x00,arg = 1,從user ap丟的
		wait_event_interruptible(wq, flag != 0);  //當前此process被放入wait queue中，等待特定事情做完後被喚醒。
		flag = 0;   //喚醒條件的flag初始化
		break;
	default:
		printk("Unknown COMMAND!!!\n");
	}
	return 0;
}

static void allocate_memory_region (void)
{
	base = ioremap_nocache(CALCULATOR_BASE , 0x1000);
	printk("Physical address: %08X\n", CALCULATOR_BASE);
	printk("After ioremap: %08X\n", (int)base);
}

static void free_allocated_memory(void)
{
	iounmap((unsigned long *) base);
}

static struct file_operations cal_fops = {
	.owner				= THIS_MODULE,
	.read 				= cal_read,
	.write 			= cal_write,
	.unlocked_ioctl 	= cal_unlocked_ioctl,
	.open 				= cal_open,
	.release 			= cal_release
};

static struct cdev cal_cdev = {
	.kobj		=	{ .name = DRIVER_NAME },
	.owner		=	THIS_MODULE,
};

static irqreturn_t cal_interrupt(int irq, void *dev_id, struct pt_regs *regs)   //ISR 所作的內容，看起來ISR並沒有特別做什麼事，只有做個count++，用來計數做了幾次這樣的中斷觸發
{																				
	//printk("received interrupt from SC_LINK\n");
													//顧名思義，當CPU接受到IRQ阜號為62的時候，就會去執行由我們所定義的ISR，
													//在此函式中，我們先將0寫入0x100的位址，正好就是我們先前虛擬硬體定義的將IRQ放掉所必須做的對應動作，
													//之後我們用wake_up_interrupt將process從waiting queue釋放出來。
													//這部份我們將會在後面與把process擺入waiting queue中一起來看/

    if (irq == IRQ_VIC_BRIDGE+PIC_BASE) {
		count++;
		printk("received interrupt from Caculator HW run %d\n",count);
		writel(0, base+0x100);				//將進行寫值到0x100，看起來好像沒寫什麼值，主要將觸使硬體calculator能irq拉下。
		printk("Release interrupt \n");
		flag = 1;		//將改變喚醒條件的flag值，使此process將能被喚醒
		wake_up_interruptible(&wq);      //將從wait queue中進行喚醒當前process
	}
    return IRQ_HANDLED;		//當接收到IRQ時，就知道硬體已經完成工作或是需要軟體執行相對應的行為讓硬體可以繼續作業下去。

}

static int __init cal_init(void)
{
	dev_t dev = 0;
	int result;

	/* register char device */
	if(CAL_MAJOR){
		dev = MKDEV(cal_major, cal_minor);   //driver register,採用新註冊機制，所有都自己建
											//此函式處理所有driver初始化的工作，我們的driver是採用char device driver完成
											//此函示定義了driver掛載上Linux後的識別碼包含Major與Minor number
											//我們實作了allocate_memory_region來向kernel要求記憶體空間，ioremap會將實體空間映射至虛擬記憶體空間。

		result = register_chrdev_region( dev, 1, DRIVER_NAME);
	}
	else{
		result = alloc_chrdev_region( &dev, cal_minor, 1, DRIVER_NAME);
		cal_major = MAJOR(dev);
	}
	if(result < 0) {
		printk(KERN_ERR DRIVER_NAME "Can't allocate major number %d for %s\n", cal_major, DEVICE_NAME);
		return -EAGAIN;
	}
	cdev_init( &cal_cdev, &cal_fops);
	result = cdev_add( &cal_cdev, dev, 1);
	if(result < 0) {
		printk(KERN_ERR DRIVER_NAME "Can't add device %d\n", dev);
		return -EAGAIN;
	}

	/* allocate memory */
    allocate_memory_region();

	/* request irq and bind ISR*/
    result = request_irq(IRQ_VIC_BRIDGE+PIC_BASE, (void *)cal_interrupt, IRQF_DISABLED, DEVICE_NAME, NULL);   //當calculator觸發中斷後，也就是做完rgb轉成y之後,qemu_irq_raise(s->irq)
	if(result){																								//在進行觸發拉起中斷，cpu收到中斷後，將進行ISR內容，就在cal_interrupt此function中
		printk(KERN_ERR DRIVER_NAME "Can't request IRQ\n");													//request_irq裡面有handler此function pointer，也就是第二個參數
	}																										//在適當的時機，也又是收到中斷觸發後，將跳轉去執行ISR

	return 0;
}

static void __exit cal_exit(void){
	dev_t dev;
	free_allocated_memory();
	dev = MKDEV(cal_major, cal_minor);
	unregister_chrdev_region( dev, 1);
	cdev_del(&cal_cdev);
	free_irq(IRQ_VIC_BRIDGE+PIC_BASE, NULL);
	printk("Calculator driver exit\n");
}

module_init(cal_init);
module_exit(cal_exit);
