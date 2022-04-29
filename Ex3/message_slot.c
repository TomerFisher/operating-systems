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

MODULE_LICENSE("GPL");

//Our custom definitions of IOCTL operations
#include "message_slot.h"

struct channel {
	long id;
	char message[BUF_LEN];
	int message_length;
}

struct channel_node {
	struct channel data;
	struct channel_node *next;
}

struct file_info {
	struct channel current_channel;
	int minor;
}

struct channel_node message_slot_channels[256];

// The message the device will give when asked
static char the_message[BUF_LEN];

//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
  int minor = iminor(inode);
  file_info* data = kmalloc(sizeof(file_info), GFP_KERNEL);
  if (data == NULL) {
	  return -1;
  }
  data->minor = minor;
  data->current_channel = message_slot_channels[minor-1]->channel;
  file->private_data = (void*) data;
  return SUCCESS;
}

static int device_release(struct inode* inode, struct file*  file) {
	kfree(file->private_data);
	return SUCCESS;
}

static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
  if (file->private_data->current_channel == NULL) {
	  errno = EINVAL;
	  return -1;
  }
  if (file->private_data->current_channel->message == NULL) {
	  errno = EWOULDBLOCK;
	  return -1;
  }
  if (length < file->private_data->current_channel->message_length) {
	  errno = ENOSPC;
	  return -1;
  }
  for (int i = 0; i < file->private_data->current_channel->message_length; i++) {
	  if (put_user(file->private_data->current_channel->message[i], buffer + i) != 0)
		  return -1;
  }
  return file->private_data->current_channel->message_length;
}

static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset)
{
  if (file->private_data->current_channel == NULL) {
	  errno = EINVAL;
	  return -1;
  }
  if (length == 0 || length > BUF_LEN) {
	  errno = EMSGSIZE;
	  return -1;
  }
  for (int i=0; i < length; i++) {
	  if (get_user(the_message[i], &buffer[i]) != 0)
		  return -1;
  }
  // on success
  for (int i=0; i < length; i++) {
	  file->private_data->current_channel->message[i] = the_message[i];
  }
  file->private_data->current_channel->message_length = length;
  return length;
  }
}

static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned int  channel_id )
{
  if( MSG_SLOT_CHANNEL != ioctl_command_id || channel_id == 0 )
  {
	errno = EINVAL;
    return FAILURE;
  }
  int minor = file->private_data->minor;
  struct channel_node node = message_slot_channels[minor-1];
  while (node != NULL) {
	  if (node->data->id == channel_id) {
		  file->private_data->current_channel = node;
		  return SUCCESS;
	  }
	  node = node->next;
  }
  node = kmalloc(sizeof(channel), GFP_KERNEL);
  node->data->id = channel_id;
  node->data->message = NULL;
  node->data->message_length = 0;
  file->private_data->current_channel = node;
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
  int rc = -1;

  // Register driver capabilities. Obtain major num
  rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );

  // Negative values signify an error
  if( rc < 0 )
  {
    printk( KERN_ALERT "%s registraion failed for  %d\n",
                       DEVICE_FILE_NAME, MAJOR_NUM );
    return rc;
  }
  // initialization all linked-list of channels for all possible message_slot to NULL
  for (int i=0; i<256; i++) {
	  message_slot_channels[i] = NULL;
  }
  return SUCCESS;
}

static void __exit simple_cleanup(void)
{
  struct channel_node* head;
  struct channel_node* tmp;
  for (int i=0; i<256; i++) {
	  head = message_slot_channels[i];
	  while(head != NULL) {
		  tmp = head;
		  head = head->next;
		  kfree(tmp);
	  }
  }
  unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

module_init(simple_init);
module_exit(simple_cleanup);
