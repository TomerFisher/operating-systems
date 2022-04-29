// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h> /* for kmalloc and kfree */

MODULE_LICENSE("GPL");

//Our custom definitions of IOCTL operations
#include "message_slot.h"

struct channel_node {
  long id;
	char message[BUF_LEN];
	int message_length;
  struct channel_node* next;
};

struct file_info {
  int minor;
	long channel_id;
  struct channel_node* channel;
};

struct channel_node* message_slot_channels[256];

// The message the device will give when asked
static char the_message[BUF_LEN];

//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode, struct file* file)
{
  int minor = iminor(inode);
  struct file_info* info = kmalloc(sizeof(struct file_info), GFP_KERNEL);
  printk("Invoking device_open: file=%p, inode=%p, minor=%d.\n", file, inode, minor);
  if (info == NULL) {
	  return FAILURE;
  }
  info->minor = minor;
  info->channel_id = 0;
  info->channel = NULL;
  file->private_data = (void*)info;
  printk("Device Open Successfully Completed.");
  return SUCCESS;
}

static int device_release(struct inode* inode, struct file*  file) {
	printk("Invoking device_release: file=%p, inode=%p.\n", file, inode);
  kfree(file->private_data);
  printk("Device Release Successfully Completed.");
	return SUCCESS;
}

static ssize_t device_read( struct file* file, char __user* buffer, size_t length, loff_t* offset)
{
  int i, message_length;
  struct file_info* info = (struct file_info*)(file->private_data);
  char* last_message;
  printk("Invoking device_read: file=%p, length=%ld.\n", file, length);
  if (info->channel_id == 0) {
	  printk("channel_id == 0\n");
    return -EINVAL;
  }
  last_message = info->channel->message;
  message_length = info->channel->message_length;
  if (message_length == 0) {
	  printk("message_length == 0\n");
    return -EWOULDBLOCK;
  }
  if (length < message_length) {
	  printk("message too long\n");
    return -ENOSPC;
  }
  for (i = 0; i < message_length; i++) {
	  if (put_user(last_message[i], buffer + i) != 0)
		  return FAILURE;
  }
  printk("The Message: %s\n", last_message);
  printk("Device Read Successfully Completed.");
  return info->channel->message_length;
}

static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset)
{
  int i=0;
  struct file_info* info = (struct file_info*)(file->private_data);
  printk("Invoking device_write: file=%p, length=%ld.\n", file, length);
  if(info->channel_id == 0) {
	  return -EINVAL;
  }
  if (length == 0 || length > BUF_LEN) {
	  return -EMSGSIZE;
  }
  for (i=0; i < length; i++) {
	  if (get_user(the_message[i], &buffer[i]) != 0)
		  return -1;
  }
  // on success
  for (i=0; i < length; i++) {
	  info->channel->message[i] = the_message[i];
  }
  info->channel->message_length = length;
  printk("Device Write Successfully Completed.");
  return length;
}

static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long channel_id)
{
  struct file_info* info = (struct file_info*)(file->private_data);
  struct channel_node* node = message_slot_channels[info->minor-1];
  printk("Invoking device_ioctl: file=%p, channel-id=%ld\n", file, channel_id);
  if (MSG_SLOT_CHANNEL != ioctl_command_id || channel_id == 0)
    return -EINVAL;
  while (node != NULL) {
	  if (node->id == channel_id) {
		  info->channel_id = channel_id;
      info->channel = node;
		  return SUCCESS;
	  }
	  node = node->next;
  }
  node = kmalloc(sizeof(struct channel_node), GFP_KERNEL);
  node->id = channel_id;
  node->message_length = 0;
  info->channel_id = channel_id;
  info->channel = node;
  printk("Device Ioctl Successfully Completed.");
  return SUCCESS;
}

//==================== DEVICE SETUP =============================

struct file_operations Fops =
{
  .owner	  = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
  .release        = device_release,
};

static int __init simple_init(void)
{
  int rc = -1, i;

  // Register driver capabilities. Obtain major num
  rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );

  // Negative values signify an error
  if( rc < 0 )
  {
    printk( KERN_ALERT "%s registraion failed for  %d\n",
                       DEVICE_RANGE_NAME, MAJOR_NUM );
    return rc;
  }
  // initialization all linked-list of channels for all possible message_slot to NULL
  for (i=0; i<256; i++) {
	  message_slot_channels[i] = NULL;
  }
  printk("Registeration Successfully Completed.");
  return SUCCESS;
}

static void __exit simple_cleanup(void)
{
  int i;
  struct channel_node* head;
  struct channel_node* tmp;
  for (i=0; i<256; i++) {
	  head = message_slot_channels[i];
	  while(head != NULL) {
		  tmp = head;
		  head = head->next;
		  kfree(tmp);
	  }
  }
  unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
  printk("Unregisteration Successfully Completed.");
}

module_init(simple_init);
module_exit(simple_cleanup);
