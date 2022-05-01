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

struct channel_node* list_of_channels[256];
struct channel_node* current_channels[256];

// The message the device will give when asked
static char the_message[BUF_LEN];

//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode* inode, struct file* file)
{
  //unsigned int* minor = kmalloc(sizeof(int), GFP_KERNEL);
  //int minor = iminor(inode);
  int* minor_pointer;
  printk("Invoking device_open(%p)\n", file);
  minor_pointer = kmalloc(sizeof(int), GFP_KERNEL);
  if (minor_pointer == NULL)
    return FAILURE;
  *minor_pointer = iminor(inode);
  file->private_data = (void*)minor_pointer;
  printk("device_open(%p) successfully completed\n", file);
  return SUCCESS;
}

static int device_release(struct inode* inode, struct file* file) {
  kfree(file->private_data);
  return SUCCESS;
}

static ssize_t device_read( struct file* file, char __user* buffer, size_t length, loff_t* offset)
{
  //int minor = file->private_data, i;
  int minor = *(int*)file->private_data, i;
  printk("Invoking device_read(%p,%ld)\n", file, length);
  if(current_channels[minor-1] == NULL)
    return -EINVAL;
  if (current_channels[minor-1]->message_length == 0)
    return -EWOULDBLOCK;
  if (length < current_channels[minor-1]->message_length)
    return -ENOSPC;
  for (i = 0; i < current_channels[minor-1]->message_length; i++) {
	  if (put_user(current_channels[minor-1]->message[i], buffer + i) != 0)
		  return FAILURE;
  }
  printk("device_read(%p,%ld) successfully completed (last written - %s)\n", file, length, current_channels[minor-1]->message);
  return current_channels[minor-1]->message_length;
}

static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset)
{
  //int minor = file->private_data, i;
  int minor = *(int*)file->private_data, i;
  printk("Invoking device_write(%p,%ld)\n", file, length);
  if(current_channels[minor-1] == NULL)
	  return -EINVAL;
  if (length == 0 || length > BUF_LEN)
	  return -EMSGSIZE;
  for (i=0; i < length; i++) {
	  if (get_user(the_message[i], &buffer[i]) != 0)
		  return -1;
  }
  for (i=0; i < length; i++) {
    current_channels[minor-1]->message[i] = the_message[i];
  }
  current_channels[minor-1]->message_length = length;
  printk("device_write(%p,%ld) successfully completed\n", file, length);
  return length;
}

static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long channel_id)
{
  //int minor = file->private_data;
  int minor = *(int*)file->private_data;
  struct channel_node* node = list_of_channels[minor-1];
  printk("Invoking device_ioctl(%p,%ld)\n", file, channel_id);
  if (MSG_SLOT_CHANNEL != ioctl_command_id || channel_id == 0)
    return -EINVAL;
  if (node == NULL) {
    node = kmalloc(sizeof(struct channel_node), GFP_KERNEL);
    node->id = channel_id;
    node->message_length = 0;
    node->next = NULL;
    current_channels[minor-1] = node;
    list_of_channels[minor-1] = node;
    printk("device_ioctl(%p,%ld) successfully completed - create new channel node\n", file, channel_id);
    return SUCCESS;
  }
  while (node->next != NULL) {
	  if (node->id == channel_id) {
      current_channels[minor-1] = node;
      printk("device_ioctl(%p,%ld) successfully completed - found channel node\n", file, channel_id);
		  return SUCCESS;
	  }
	  node = node->next;
  }
  if (node->id == channel_id) {
	  current_channels[minor-1] = node;
    printk("device_ioctl(%p,%ld) successfully completed - found channel node\n", file, channel_id);
	  return SUCCESS;
  }
  node->next = kmalloc(sizeof(struct channel_node), GFP_KERNEL);
  node->next->id = channel_id;
  node->next->message_length = 0;
  node->next->next = NULL;
  current_channels[minor-1] = node->next;
  printk("device_ioctl(%p,%ld) successfully completed - create new channel node\n", file, channel_id);
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
	  list_of_channels[i] = NULL;
    current_channels[i] = NULL;
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
	  head = list_of_channels[i];
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
