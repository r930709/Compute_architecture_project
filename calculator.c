/*
 *
 * calculator Test Hardware
 *
-------1. no operator we need
-------2. add operand_1,operand_2,operand_3 to store the rgb information
-------3. must care the address(0x80000000~0x8000FFFF)we can use
-------4. we don't need to fix the read(),just need to fix write()
-------5. the read function is mapping address,we don't need to chage the address,specially,the 0XFE4
*/

#include "sysbus.h"


int operand_1;	// 0x80000004
int operand_2;	// 0x80000008
int operand_3;	// 0x8000000C

int result;		// 0x80000010
int go;			// 0x80000000



typedef struct {
    SysBusDevice busdev;
    qemu_irq irq;
    const unsigned char *id;
} calculator_state;

static void calculate(calculator_state *s)
{


   result = operand_1*0.299 + operand_2*0.587 + operand_3*0.114;

   	qemu_irq_raise(s->irq);				//特定事情做完後拉起中斷
	go = 0;
}

static const VMStateDescription vmstate_calculator = {
	.name = "calculator",
	.version_id = 1,
	.minimum_version_id = 1,
	.fields = (VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};
//由於我們是以ARM為開發環境，所以ARM Linux在註冊device driver時會先確認硬體編號，此硬體編碼設定在0XFE0 ~0XFFC。
//我們定義在calculator_id_arm[8]中，並且當CPU要求這段位址時，我們將id送回去。ARM device的ID末四位元組一樣
static const unsigned char calculator_id_arm[8] =
  { 0x11, 0x10, 0x88, 0x08, 0x0d, 0xf0, 0x05, 0xb1 };

//前面有提過，CPU只要讀取base+ 0x1000之間的位址就會來執行此函式。我們採用switch來做一個簡單的decoder

static uint32_t calculator_read(void *opaque, target_phys_addr_t offset)
{
	printf("lenadriver read from %d!\n", offset);
	switch(offset & 0xFFF){
	case 0XFE0:
		return calculator_id_arm[0];
		break;
	case 0XFE4:
		return calculator_id_arm[1];
		break;
	case 0xFE8:
		return calculator_id_arm[2];
		break;
	case 0XFEC:
		return calculator_id_arm[3];
		break;
	case 0XFF0:
		return calculator_id_arm[4];
		break;
	case 0xFF4:
		return calculator_id_arm[5];
		break;
	case 0XFF8:
		return calculator_id_arm[6];
		break;
	case 0XFFC:
		return calculator_id_arm[7];
		break;
	default:
		if((offset & 0xFFF) == 0x10)
			return result;
		else{
			printf(" non-readable address!!!\n");
			return 0;
		}
	}
}
//與read function類似，但是input除了offset外還多了要寫入的value。我們一樣採用switch設計decoder，透過offset的判斷，我們依照一開始所訂下的register和memory位址來寫入value
static void calculator_write(void *opaque, target_phys_addr_t offset, uint32_t value)
{
	calculator_state *s = (calculator_state *)opaque;

	printf("lenadriver write %d to %d!\n", value, offset);

	switch(offset & 0xFFF){     //處理了interrupt的部分，我們可看到當go被設定為1時，write function會去觸發calculate並依據operator的定義完成對應的計算。
								//計算完成之後將interrupt拉起。
								//CPU接收到interrupt之後會觸發ISR，我們設定ISR處理完成後會寫值到0X100的register，此時write function再將IRQ拉下以完成中斷處理。

	case 0x0:
		go = value;   //value = 1
		if(value != 0)
			calculate(s);
		break;
	case 0x4:
		operand_1 = value;
		printf("operand1 : %d\n",operand_1);
		break;
	case 0x8:
		operand_2 = value;
		printf("operand2 : %d\n",operand_2);
		break;
	case 0xC:
		operand_3 = value;
		printf("operand3 : %d\n",operand_3);
		break;

	case 0x100:
		qemu_irq_lower(s->irq);			//激活driver process有進入ISR後拉下中斷
		printf("ISR is triggered!!!\n");
		break;
	default:
		printf("invalid address!!!\n");
	}
}
// const在*右邊，表示所指向的位址不能變,並搭配array 來用index的方法記住function pointer，指向function
static CPUReadMemoryFunc * const calculator_readfn[] = {       //readfn與writefn為函示陣列，陣列中三個finction為byte-RW, half-RW和word-RW。
															   //我們設定全部的RW皆以word為單位，所以全指向同一個RW函式

   calculator_read,
   calculator_read,
   calculator_read
};
// const在*右邊，表示所指向的位址不能變,並搭配array 來用index的方法記住function pointer，指向function
static CPUWriteMemoryFunc * const calculator_writefn[] = {
   calculator_write,
   calculator_write,
   calculator_write
};


static int calculator_init(SysBusDevice *dev, const unsigned char *id)
{
    int iomemtype;
    calculator_state *s = FROM_SYSBUS(calculator_state, dev);

    iomemtype = cpu_register_io_memory(calculator_readfn,       //註冊了才能用,calculator_readfn,calculator_writefn 此兩個function pointer array
                                       calculator_writefn, s,
                                       DEVICE_NATIVE_ENDIAN);

    sysbus_init_mmio(dev, 0x1000,iomemtype);   //mmio ，可以確定是硬體設計上使用了memory mapping i/o的機制
    sysbus_init_irq(dev, &s->irq);             //註冊0x1000。假設我們將此虛擬硬體掛載在0x80000000，那CPU存取 0x80000000 ~ 0x80000FFF就會使用上面定義的RW函式。
	//註冊虛擬硬體的IRQ阜號

    s->id = id;


	operand_1 	= 0;
	operand_2	= 0;
    operand_3	= 0;
	go			= 0;
	result		= 0;

    printf("lenadriver Hardware Initialization! \n");

    return 0;
}

static int calculator_init_arm(SysBusDevice *dev)
{
    return calculator_init(dev, calculator_id_arm);
}

static SysBusDeviceInfo calculator_hw_info = {
    .init = calculator_init_arm,
    .qdev.name  = "calculator",
    .qdev.size  = sizeof(calculator_state),
    .qdev.vmsd = &vmstate_calculator,
};

static void calculator_register_devices(void)
{
	sysbus_register_withprop(&calculator_hw_info);
}

device_init(calculator_register_devices)    //device_init整個虛擬硬體初始化的源頭


