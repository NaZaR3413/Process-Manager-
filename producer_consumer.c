#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/tty.h>
#include <linux/uidgid.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/param.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/time_namespace.h>
#include <linux/time.h>

#include <linux/proc_fs.h>
#include <linux/slab.h>

#define MAX_BUFFER_SIZE 500
#define MAX_NO_OF_PRODUCERS 1
#define MAX_NO_OF_CONSUMERS 100

#define PCINFO(s, ...) pr_info("###[%s]###" s, __FUNCTION__, ##__VA_ARGS__)

unsigned long long total_time_elapsed = 0;

int total_no_of_process_produced = 0;
int total_no_of_process_consumed = 0;

int end_flag = 0;

char producers[MAX_NO_OF_PRODUCERS][12] = {"kProducer-X"};
char consumers[MAX_NO_OF_CONSUMERS][12] = {"kConsumer-X"};

static struct task_struct *ctx_producer_thread[MAX_NO_OF_PRODUCERS];
static struct task_struct *ctx_consumer_thread[MAX_NO_OF_CONSUMERS];

// use fill and use to keep track of the buffer
struct task_struct **buffer;
int fill = 0;
int use = 0;

static int buffSize = 1000;
static int prod = 0;
static int cons = 0;
static int uuid = 0;

module_param(buffSize, int, 0);
module_param(prod, int, 0);
module_param(cons, int, 0);
module_param(uuid, int, 0);


static struct semaphore mutex;
static struct semaphore full;
static struct semaphore empty;

int producer_thread_function(void *pv)
{
    struct task_struct *task;
    int index = 0;

    for_each_process(task)
    {
        if (task->cred->uid.val == uuid)
        {
            
            if(down_interruptible(&empty)) {break;}
            if(down_interruptible(&mutex)) {break;}
            if(kthread_should_stop()) {break;}
            index = (total_no_of_process_produced) % buffSize;
            buffer[index] = task;
            total_no_of_process_produced++;
            PCINFO("[kProducer-1] Produce-Item#:%d at buffer index: %d for PID:%d \n",
                   total_no_of_process_produced, index, task->pid);
        }
        up(&mutex);
        up(&empty);
    }

    PCINFO("Producer Thread stopped.\n");
    return 0;
}

int consumer_thread_function(void *pv)
{
    int no_of_process_consumed = 0;
    struct task_struct *task;
    int* threadID = (int*)pv;
    int index = 0;

    while (!kthread_should_stop())
    {
        if(down_interruptible(&full)) {break;}
        if(down_interruptible(&mutex)) {break;}
        
        index = total_no_of_process_consumed % buffSize;
        task = buffer[index];
        if(kthread_should_stop()) {break;}
        if(end_flad == 1) {break;}

        unsigned long long ktime = ktime_get_ns();
        unsigned long long process_time_elapsed = (ktime - task->start_time) / 1000000000;
        total_time_elapsed += ktime - task->start_time;

        unsigned long long process_time_hr = process_time_elapsed / 3600;
        unsigned long long process_time_min = (process_time_elapsed - 3600 * process_time_hr) / 60;
        unsigned long long process_time_sec = (process_time_elapsed - 3600 * process_time_hr) - (process_time_min * 60);

        no_of_process_consumed++;
        total_no_of_process_consumed++;
        printk(KERN_INFO"[kConsumer-%d] Consumed Item#-%d on buffer index:%d::PID:%lu \t Elapsed Time %llu:%llu:%llu \n", *threadID, no_of_process_consumed, index, task->pid, process_time_hr, process_time_min, process_time_sec);
        up(&mutex);
        up(&empty);
    }

    PCINFO("[%s] Consumer Thread stopped.\n", current->comm);
    return 0;
}

static int thread_init_module(void)
{
    PCINFO("CSE330 Project-2 Kernel Module Inserted\n");
    PCINFO("Kernel module received the following inputs: UID:%d, Buffer-Size:%d, No of Producer:%d, No of Consumer:%d", uuid, buffSize, prod, cons);
    buffer = kmalloc(buffSize * sizeof(struct task_struct*), GFP_KERNEL);
    if(!buffer){
        return -ENOMEM;
    }
    
    if (buffSize > 0 && (prod >= 0 && prod < 2) && cons > 0)
    {
        // TODO initialize the semaphores here
        sema_init(&mutex, 1);
        sema_init(&full, 0);
        sema_init(&empty, buffSize);

        ctx_producer_thread[0] = kthread_run(producer_thread_function, NULL, "Producer");

        //Check to see if producer thread was created successfully
        if(!IS_ERR(ctx_producer_thread))
        {
            printk(KERN_INFO "[kProducer-1] kthread Producer Created Successfully");
        }
        
        for(index = 0; index < cons; index++)
        {
            int* threadID = kmalloc(sizeof(int), GFP_KERNEL);
            *threadID = index+1;
            ctx_consumer_thread[index] = kthread_run(consumer_thread_function, (void*)threadID, "Consumer");
            if(!IS_ERR(ctx_consumer_thread[index]))
            {
                printk(KERN_INFO "[kConsumer-%d] kthread Consumer Created Successfully\n", index);
            }
        }
    }
    else
    {
        // Input Validation Failed
        PCINFO("Incorrect Input Parameter Configuration Received. No kernel threads started. Please check input parameters.");
        PCINFO("The kernel module expects buffer size (a positive number) and # of producers(0 or 1) and # of consumers > 0");
    }

    return 0;
}

static void thread_exit_module(void)
{
    if (buffSize > 0)
    {
        while (1)
        {
            if (total_no_of_process_consumed == total_no_of_process_produced || !cons || !prod)
            {
                if (!cons)
                {
                    up(&empty);
                }

                for (int index = 0; index < prod; index++)
                {
                    if (ctx_producer_thread[index])
                    {
                        kthread_stop(ctx_producer_thread[index]);
                    }
                }

                end_flag = 1;

                for (int index = 0; index < cons; index++)
                {
                    up(&full);
                    up(&mutex);
                }

                for (int index = 0; index < cons; index++)
                {
                    if (ctx_consumer_thread[index]){
                        kthread_stop(ctx_consumer_thread[index]);
                    }
                }
                break;
            }
            else
                continue;
        }

        // total_time_elapsed is now in nsec
        total_time_elapsed = total_time_elapsed / 1000000000;

        unsigned long long total_time_hr = total_time_elapsed / 3600;
        unsigned long long total_time_min = (total_time_elapsed - 3600 * total_time_hr) / 60;
        unsigned long long total_time_sec = (total_time_elapsed - 3600 * total_time_hr) - (total_time_min * 60);

        printk(KERN_INFO"Total number of items produced: %d", total_no_of_process_produced);
        printk(KERN_INFO"Total number of items consumed: %d", total_no_of_process_consumed);
        printk(KERN_INFO"The total elapsed time of all processes for UID %d is \t%llu:%llu:%llu  \n", uuid, total_time_hr, total_time_min, total_time_sec);
    }

    PCINFO("CSE330 Project 2 Kernel Module Removed\n");
}

module_init(thread_init_module);
module_exit(thread_exit_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nilay Patel, Eyasu Abebe, Leobardo Montes De Oca Torres, Aidan Daly");
MODULE_DESCRIPTION("CSE330 Project 2\n");
MODULE_VERSION("0.1");