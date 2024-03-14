#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/semaphore.h>
#include <linux/time.h>


static int buffSize = 0;
static int prod = 0;
static int cons = 0;
static int uuid = 0;
//static struct task_struct *producer_thread, *consumer_thread;
module_param(buffSize, int, 0);
module_param(prod, int, 0);
module_param(cons, int, 0);
module_param(uuid, int, 0);


static struct task_struct *buffer[200];
static struct task_struct *user_process[2000];
static struct task_struct *p_proc[2000];
static struct task_struct *c_proc[2000];
static u64 total_time = 0;
static int fill = 0;
static int use = 0;

static size_t p_item = 0;
static size_t c_item = 0;
static size_t proc_count =0;

static struct semaphore mutex;
static struct semaphore full;
static struct semaphore empty;

static int producer_thread(void *arg)
{
  int *process;
  process = (int *)arg;

  while(!kthread_should_stop())
    {
      if(down_interruptible(&empty)){break;}
      if(kthread_should_stop())
	{break;}
      if(down_interruptible(&mutex))
	{break;}
      if(kthread_should_stop())
	{break;}
      if(p_item<proc_count)
	{buffer[fill] = user_process[p_item];
	  printk("[Producer-%d] Produced Item#-%zu at buffer index:%d for PID:%d\n",(*process+1),p_item+1, fill,user_process[p_item]->pid);
	  fill = (fill+1)%buffSize;
	  p_item++;
	}
      up(&mutex);
      up(&full);
    }

    return 0;
    
    
}
  static int consumer_thread(void *arg)
  {
    u64 time = 0;
    u64 hour = 0;
    u64 min = 0;
    u64 sec = 0;
    int con;

    con = (int *)arg;

    while(!kthread_should_stop())
      {
	if(down_interruptible(&full))
	  {break;}
	if(kthread_should_stop())
	  {break;}
	if(down_interruptible(&mutex))
	  {break;}
	if(kthread_should_stop())
	  {break;}
	if(c_item<proc_count)
	  {
	    time = ktime_get_ns()-buffer[use]->start_time;
	    sec = time/1000000000;
	    min = sec/60;
	    min = min%60;
	    sec = sec%60;
	    total_time = total_time+time;
	    printk("[Consumer-%d] Consumed Item#-%zu on buffer index:%d PID:%d Elapsed time-%llu:%llu:%llu\n",("con+1),c_item+1,use,buffer[use]->pid,hour, minutes, seconds"));
	    use = (use+1)%buffSize;
	    c_item++;
	  }
	up(&mutex);
	up(&empty);
      }
    return 0;
  }
  int producer_consumer_init(void)
  { int i;
    static int producer[10];
    static int consumer[10];
    struct task_struct *tasks;

    for_each_process(tasks){
      if(tasks-> cred -> uid.val==uuid){
	user_process[proc_count] = tasks;
	++proc_count;
      }
    }
    sema_init(&mutex, 1);
    sema_init(&full, 0);
    sema_init(&empty, buffSize);

    for(i=0;i<prod;i++)
      {
	producer[i] = i;
	p_proc[i] = kthread_run(producer_thread, (void *)&producer[i], "producer");
      }
    for(i=0;i<cons;i++)
      {
	consumer[i] = i;
	c_proc[i] = kthread_run(consumer_thread, (void *)&consumer[i], "consumer");
      }
    return 0;
  }

  void producer_consumer_exit(void)
  {
    int a = 0;
    u64 hr = 0;
    u64 min = 0;
    u64 sec = 0;
    for(a=0; a < prod; a++)
      {
	up(&mutex);
	up(&empty);
	kthread_stop(p_proc[a]);
      }
    for(a=0;a<cons;a++)
      {
	up(&mutex);
	up(&full);
	kthread_stop(c_proc[a]);
      }
    hr    = min/60;
    min = sec/60;
    sec = total_time/1000000000;
    min = min%60;
    sec= sec%60;
    printk("Total number of items produced: %d", p_item);
    printk("Total number of items consumed: %d", c_item);
    printk("The total elapsed time of all processes for UID %d is %llu:%llu:%llu\n", uuid, hr,min,sec);
  }

  module_init(producer_consumer_init);
  module_exit(producer_consumer_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nilay Patel, Eyasu Abebe, Leobardo Montes De Oca Torres, Aidan Daly");
MODULE_DESCRIPTION("CSE330 2024 Spring Project 2 Process Management\n");
MODULE_VERSION("0.1");