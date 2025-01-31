#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/uaccess.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Parmeet Singh");
MODULE_DESCRIPTION("Kprobe ioctl version");
struct ProcessInfo
{
        int pid;
        int syscall;
}__attribute__((__packed__));



#define SET_MODE _IOW('a','a',int)
#define SET_BLOCK_SYSCALL _IOW('b','b',struct ProcessInfo*)
#define SET_BLOCK_PID _IOW('a',2,int)
#define SET_LOG_SYSCALL_FSM _IOW('d','d',int)
#define GET_LOG_SYSCALL_FSM _IOR('e','e',int)



#define MODE_OFF 0
#define MODE_LOG 1
#define MODE_BLOCK 2
#define MODE_LOG_FSM 3
#define MAJOR_NO 250
#define MINOR_NO 0

static int mode = MODE_OFF;
static int block_syscall =-1;
static int log_syscall_fsm=-1;
static pid_t block_pid =-1;
static int major_number;
static struct class *dev_class;
static struct ProcessInfo kernel_process_info;

static struct kprobe kp_open, kp_read, kp_write;
static int syscall_observed =-1;
// implement writing to file
//

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	syscall_observed=-1;
	if(mode == MODE_OFF)
	{

		printk(KERN_INFO "Module is in disable mode with mode %d \n",mode);
	}

	if(mode == MODE_LOG)
	{
		printk(KERN_INFO "Module is in LOG mode with mode %d \n",mode);

		printk(KERN_INFO "(PID : %d) with system call :: %s \n",current->pid,p->symbol_name);

	}
	if(mode == MODE_LOG_FSM)
	{
		printk(KERN_INFO "Module is in LOG FSM MODE %d \n",mode);
		
		int local_syscall=-1;
                if( strcmp(p->symbol_name, "do_sys_open")==0)
                {
			printk(KERN_INFO "Module is in LOG FSM MODE OPEN with symbol name as  %s \n",p->symbol_name);
                        local_syscall=0;
			syscall_observed=0;
                }
                else if( strcmp(p->symbol_name,"vfs_read")==0)
                {
			printk(KERN_INFO "Module is in LOG FSM MODE READ with symbol name as  %s \n",p->symbol_name);
                        local_syscall=1;
			syscall_observed=1;
                }
                else if( strcmp (p->symbol_name , "vfs_write")==0)
                {
			printk(KERN_INFO "Module is in LOG FSM MODE WRITE with symbol name as  %s \n",p->symbol_name);
                        local_syscall=2;
			syscall_observed=2;
                }
		if(local_syscall == log_syscall_fsm)
		{
			printk(KERN_INFO "Module is in LOG FSM MODE IF Cond  with symbol name as  %s \n",p->symbol_name);
			printk(KERN_INFO "(PID : %d) with system call :: %s \n",current->pid,p->symbol_name);
		}
	
	}

	if(mode == MODE_BLOCK )
	{

		if(block_syscall==-1)
		{
			printk(KERN_INFO "Blocking the specific PID %d \n",block_pid);
			if(current->pid == block_pid)
                        {
                                printk(KERN_INFO "(PID : %d) with system call :: %s is blocked \n",current->pid,p->symbol_name);
                                return -1;
                        }
		}
		else
		{
			printk(KERN_INFO "Module is in BLOCK Mode  with mode  %d \n",mode);
			int local_syscall=-1;
			if(strcmp(p->symbol_name , "do_sys_open")==0)
			{
				printk(KERN_INFO "Module is in BLOCK Mode OPEN with symbol name as  %s \n",p->symbol_name);
				local_syscall=0;
			}
			else if(strcmp(p->symbol_name,"vfs_read")==0)
			{
				printk(KERN_INFO "Module is in BLOCK Mode READ with symbol name as  %s \n",p->symbol_name);
				local_syscall=1;
			}
			else if(strcmp(p->symbol_name, "vfs_write")==0)
			{
				printk(KERN_INFO "Module is in BLOCK Mode WRITE with symbol name as  %s \n",p->symbol_name);
				local_syscall=2;
			}
			printk(KERN_INFO "Module is in BLOCK Mode with details as follow  %d and %s \n",current->pid, local_syscall);
	
			if(current->pid == kernel_process_info.pid && local_syscall == kernel_process_info.syscall)
			{
				printk(KERN_INFO "Process :: %s (PID : %d) with system call :: %s is blocked \n",current->comm,current->pid,p->symbol_name);
         	       		return -1;
			}
		}

	}

	return 0;

}


static long control_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	
	printk(KERN_INFO "SWITCHING BETWEEN THE IOCTL CALL %d \n",cmd);
	switch(cmd)
	{
		case SET_MODE:
			if(arg >=2)
			{
				printk(KERN_ERR "Error argument list is greater or equal to 2\n");	
				return -EINVAL;
			}
			mode = arg;
			printk(KERN_INFO "IOCTL :: kprobe mode set to %d\n",mode);
			break;
		case SET_BLOCK_SYSCALL:
			if(arg >=2)
                        {
                                printk(KERN_ERR "Error argument list is greater or equal to 2\n");
                                return -EINVAL;
                        }

			printk(KERN_INFO "IOCTL :: Entering the block system call before copying the data from user \n",mode);
			if(copy_from_user(&kernel_process_info,(struct ProcessInfo __user *)arg,sizeof(kernel_process_info)))
			{
				printk(KERN_ERR "Failed to copy data from user space\n");
				return -EFAULT;
			
			}
			printk(KERN_INFO "Recieved pid %d for syscall %d ",kernel_process_info.pid,kernel_process_info.syscall);
			block_pid=kernel_process_info.pid;
			block_syscall=kernel_process_info.syscall;
			mode =MODE_BLOCK;
			printk(KERN_INFO "IOCTL :: IOCTL Blocked System Call for mode is set to %d %d\n",block_syscall,mode);
                        break;
		case SET_BLOCK_PID:
			if(arg >=2)
                        {
                                printk(KERN_ERR "Error argument list is greater or equal to 2\n");
                                return -EINVAL;
                        }
			block_pid =arg;
			printk(KERN_INFO "IOCTL :: IOCTL Blocked Process Call for %d\n",block_pid);
			break;
		case SET_LOG_SYSCALL_FSM:
			if(arg >=2)
                        {
                                printk(KERN_ERR "Error argument list is greater or equal to 2\n");
                                return -EINVAL;
                        }
			printk(KERN_INFO "IOCTL :: Entering the SET LOG SYSCALL FOR FSM \n");
			mode = MODE_LOG_FSM;
			log_syscall_fsm = arg;
			printk(KERN_INFO "IOCTL :: IOCTL Loggin FSM for system call :: %d :: mode %d",log_syscall_fsm, mode);
			break;
		case GET_LOG_SYSCALL_FSM:
			printk(KERN_INFO "IOCTL :: Entering the GET LOG SYSCALL FOR FSM \n");
			if(syscall_observed <0)
			{
				printk(KERN_INFO "No system call observered returing negative value\n");
				return -EAGAIN;
			}
			else if(syscall_observed >=0)
			{
				syscall_observed =0;
				if(copy_to_user((int *)arg,&syscall_observed,sizeof(syscall_observed)))
				{
					return -EFAULT;
				}
				printk(KERN_INFO "Sent value back to user space ");
			}
			break;

		default : return -EINVAL;
		
	}
	return 0;
}

static int device_open(struct inode *inode,struct file *file)
{
	printk(KERN_INFO "Device driver module is opened\n");
	return 0;
}

static struct file_operations fops={
	.open = device_open,
	.unlocked_ioctl = control_ioctl
};
 

static int __init kprobe_module_init(void)
{
	int ret;
	major_number = register_chrdev(0,"Char_device_driver",&fops);
	if(major_number <0)
	{
		printk(KERN_ERR "Init ::: Failed to initialize character device driver \n");
		return major_number;
	}
	printk(KERN_INFO "Device driver registering \n");

	dev_class = class_create("Char_device_driver_class");
	if(IS_ERR(dev_class))
	{
		unregister_chrdev(major_number,"Char_device_driver");
		return PTR_ERR(dev_class);
	}

	if(IS_ERR(device_create(dev_class,NULL,MKDEV(major_number,0),NULL,"Char_device_driver")))
	{
		class_destroy(dev_class);
		unregister_chrdev(major_number,"Char_device_driver");
		return PTR_ERR(dev_class);
	}


        kp_read.symbol_name = "vfs_read";
        kp_read.pre_handler = handler_pre;

	printk(KERN_INFO "Devicce driver registerred \n");
        ret = register_kprobe(&kp_read);
	printk(KERN_INFO "Registering the kp read module :: RET :: %d \n", ret);
	//TODO:: THIS CLEANUP CAN BE IMPROVED USING GENERALISED METHOD
        if(ret<0)
        {
                printk(KERN_ERR "Failed to register kprobe for system read\n");
		device_destroy(dev_class,MKDEV(major_number,0));
		class_destroy(dev_class);
                unregister_chrdev(major_number,"Char_device_driver");
                return ret;
        }

        kp_open.symbol_name="do_sys_open";
        kp_open.pre_handler= handler_pre;


        ret = register_kprobe(&kp_open);
	//TODO:: SAME HERE
	printk(KERN_INFO "Registering the kp open module :: RET :: %d \n", ret);
        if(ret <0)
        {
                printk(KERN_ERR "Failed to register kprobe for system open\n");
                unregister_kprobe(&kp_read);
		device_destroy(dev_class,MKDEV(major_number,0));
                class_destroy(dev_class);
                unregister_chrdev(major_number,"Char_device_driver");
                return ret;
        }

        kp_write.symbol_name = "vfs_write";
        kp_write.pre_handler = handler_pre;

        ret = register_kprobe(&kp_write);

	printk(KERN_INFO "Registering the kp write module :: RET :: %d \n", ret);

	//TODO:: SAME HERE ALSO
        if(ret <0)
        {
                printk(KERN_ERR "Failed to register kprobe for system write\n");
                unregister_kprobe(&kp_read);
                unregister_kprobe(&kp_open);
		device_destroy(dev_class,MKDEV(major_number,0));
                class_destroy(dev_class);
                unregister_chrdev(major_number,"Char_device_driver");
                return ret;
        }
	
	printk(KERN_INFO "kprobe loaded successfully initialisation is done \n");
	return ret;
}


static void __exit kprobe_module_exit(void)
{
	printk(KERN_INFO "Exiting to deregister kprobe modulez\n");
        unregister_kprobe(&kp_read);
        unregister_kprobe(&kp_open);
	unregister_kprobe(&kp_write);
        device_destroy(dev_class,MKDEV(major_number,0));
       	class_destroy(dev_class);
	unregister_chrdev(major_number,"Char_device_driver");
	printk(KERN_INFO "Exiting to deregister kprobe module done\n");
}


module_init(kprobe_module_init);
module_exit(kprobe_module_exit);

