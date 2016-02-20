#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>	
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include "mp1_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("31");
MODULE_DESCRIPTION("CS-423 MP1");

#define MAX_PROC_SIZE 1024
#define FILENAME "status"
#define DIRECTORY "mp1"
#define DEBUG 1

LIST_HEAD(list_head);

static struct proc_dir_entry* proc_dir;
static struct proc_dir_entry* proc_entry;
static struct workqueue_struct* work_queue;
static struct timer_list timer ;
static char *procfs_buffer;
static struct semaphore sem;


typedef struct pid_list
{
	struct list_head list;
	pid_t  pid;
	unsigned long cpu_time;
	void* data;
}pid_node;


void add_node(pid_t pid)
{
	pid_node* proc;
	proc=kmalloc(sizeof(pid_node), GFP_KERNEL);
	if(!proc) return;
	proc->pid=pid;
	proc->cpu_time=0;
	list_add_tail(&proc->list, &list_head);
}

static void work_queue_handler(struct work_struct *work)
{
	pid_node *curr;
	pid_node *q;
	down(&sem);	
	list_for_each_entry_safe(curr, q, &list_head, list)
	{
		int ret;
		unsigned long cpu_time;
		ret=get_cpu_use(curr->pid, &cpu_time);
		if(ret<0) 
		{
			list_del(&curr->list);
			kfree(curr);
		}
		else
		{
			curr->cpu_time=cpu_time;
		}
	}		
	up(&sem);	
	kfree(work);
}

void jit_timer_fn(unsigned long arg)
{
	struct work_struct* work=kmalloc(sizeof(struct work_struct), GFP_KERNEL);
	INIT_WORK(work, work_queue_handler);
	queue_work(work_queue, work);	
	mod_timer(&timer, jiffies+msecs_to_jiffies(5000));
}


ssize_t mp1_read(struct file * file, char __user *buffer, size_t buffer_length, loff_t *data)
{
	int copied=0;
	procfs_buffer = (char*)kmalloc(buffer_length*sizeof(char), GFP_KERNEL);	
	if(*data>0) return copied;
	procfs_buffer[0]=0;
	down(&sem);	
	{
		pid_node *curr;
		list_for_each_entry(curr, &list_head, list)
		{
			copied+=sprintf(procfs_buffer, "%s%d: %lu\n", procfs_buffer, curr->pid, curr->cpu_time);
		}
	}
	up(&sem);
	#ifdef DEBUG
	printk(KERN_ALERT "=================================================");
	printk(KERN_ALERT "\n\n\n\n %s\n\n\n\n", procfs_buffer);
	#endif
	if(copied>buffer_length)
	{
		printk(KERN_ALERT "WARNING: NOT ENOUGH BUFFER \n");
		copy_to_user(buffer, procfs_buffer, buffer_length);
		copied = buffer_length;
	}
	else
	{
		copy_to_user(buffer, procfs_buffer, strlen(procfs_buffer)+1);
		copied = strlen(procfs_buffer)+1;
	}
	#ifdef DEBUG
	printk(KERN_ALERT "\n\n %s\n\n", buffer);
	printk(KERN_ALERT "=================================================");
	#endif
	kfree(procfs_buffer);
	return copied;
}

ssize_t mp1_write(struct file* file, const char __user*  buffer, size_t count, loff_t* data)
{
	long int pid;
	procfs_buffer=(char*)kmalloc((count+1)*sizeof(char), GFP_KERNEL);
	copy_from_user(procfs_buffer, buffer, count);
	procfs_buffer[count]='\0';

	//pid = *(int*)procfs_buffer;
	kstrtol(procfs_buffer,10,&pid);
	down(&sem);	
	{
		pid_node* curr;
		curr=(struct pid_list *)kmalloc(sizeof(pid_node), GFP_KERNEL);
		curr->pid=pid;
		curr->cpu_time=0;
		list_add_tail(&curr->list, &list_head);
	}
	up(&sem);

	kfree(procfs_buffer);
	return count;
}

static const struct file_operations mp1_file = {
	.owner = THIS_MODULE,
	.read = mp1_read,
	.write = mp1_write,
};

static int __init mp1_module_init(void)
{

   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE LOADING\n");
   #endif
   
   if ((proc_dir = proc_mkdir(DIRECTORY, NULL)) == NULL)
   {
	   printk(KERN_ALERT "MP1 MODULE DIR CREATION FAIL\n");
	   remove_proc_entry(DIRECTORY, NULL);
		return -ENOMEM;
   }
   if ((proc_entry = proc_create(FILENAME, 0666, proc_dir, &mp1_file)) == NULL)
   {
	   printk(KERN_ALERT "MP1 MODULE FILE CREATION FAIL\n");
	   remove_proc_entry(FILENAME, proc_dir);
		return -ENOMEM;
   }
	if(!(work_queue = create_workqueue("FUCK TASKLET")))
	{
		printk(KERN_ALERT "WORK QUEUE INITIATION FAILURE \n");
		return -ENOMEM;
	}
	sema_init(&sem, 1);
	init_timer(&timer);
	setup_timer(&timer, jit_timer_fn, 0);
	mod_timer(&timer, jiffies+msecs_to_jiffies(5000));
	if (DEBUG)printk(KERN_ALERT "MP1 MODULE LOADED\n");

	return 0;
}


static void __exit mp1_module_exit(void)
{
	pid_node* curr;
	pid_node* q;
   remove_proc_entry(FILENAME, proc_dir);
   remove_proc_entry(DIRECTORY, NULL);

	if(del_timer(&timer)) printk(KERN_INFO "TIME DELETE FAILURE \n");

	flush_workqueue(work_queue);
	destroy_workqueue(work_queue);

	
	list_for_each_entry_safe(curr, q, &list_head, list)
	{
		down(&sem);		
		{
			list_del(&curr->list);
			kfree(curr);
		}
		up(&sem);
	}
 	if (DEBUG)printk(KERN_ALERT "MP1 MODULE UNLOADED\n");

}

module_init(mp1_module_init);
module_exit(mp1_module_exit);


