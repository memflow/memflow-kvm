#include <linux/types.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <linux/kvm_host.h>
#include <linux/miscdevice.h>
#include "mabi.h"
#include "main.h"
#include "ksyms.h"

MODULE_DESCRIPTION("memflow kernel module used to support KVM backend");
MODULE_AUTHOR("Heep");
MODULE_LICENSE("GPL");

static int memflow_init(void);
static void memflow_exit(void);

module_init(memflow_init);
module_exit(memflow_exit);

static long memflow_ioctl(struct file *filp, unsigned int cmd, unsigned long argp);

static const struct file_operations memflow_chardev_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = memflow_ioctl
};

static const struct miscdevice memflow_dev = {
	MEMFLOW_IOCTL_MAGIC,
	"memflow",
	&memflow_chardev_ops
};

static int memflow_vm_release(struct inode *inode, struct file *filp);
static long memflow_vm_ioctl(struct file *filp, unsigned int cmd, unsigned long argp);
static int memflow_vm_mmap(struct file *file, struct vm_area_struct *vma);

static struct file_operations memflow_vm_fops = {
	.release = memflow_vm_release,
	.unlocked_ioctl = memflow_vm_ioctl,
	.mmap = memflow_vm_mmap
};

KSYMDEF(insert_vm_struct);
KSYMDEF(vm_area_alloc);
KSYMDEF(vm_area_free);
KSYMDEF(kvm_lock);
KSYMDEF(vm_list);

static int memflow_init(void)
{
	struct kvm *kvm;
	struct kvm_memslots *slots;
	struct kvm_memory_slot *slot;
	int i;
	int r;

	mprintk("initializing...\n");

	r = misc_register(&memflow_dev);

	if (r)
		return r;

	KSYMINIT(insert_vm_struct);
	KSYMINIT(vm_area_alloc);
	KSYMINIT(vm_area_free);
	KSYMINIT(kvm_lock);
	KSYMINIT(vm_list);

	mprintk("initialized\n");

	mprintk("kvm list:\n");

	mutex_lock(_kvm_lock);

	list_for_each_entry(kvm, _vm_list, vm_list) {
		kvm_get_kvm(kvm);
		mutex_lock(&kvm->lock);
		mutex_lock(&kvm->slots_lock);
		mprintk("kvm ptr: %p\n", kvm);
		mprintk("memslots:");

		slots = kvm_memslots(kvm);

		for (i = 0; i < slots->used_slots; i++) {
			slot = slots->memslots + i;
			mprintk("%d %lx %lx %llx\n", i, slot->userspace_addr, gfn_to_gpa(slot->npages), gfn_to_gpa(slot->base_gfn));
		}

		mutex_unlock(&kvm->slots_lock);
		mutex_unlock(&kvm->lock);
		kvm_put_kvm(kvm);
	}

	mutex_unlock(_kvm_lock);

	return 0;
}

static void memflow_exit(void)
{
	misc_deregister(&memflow_dev);
	mprintk("uninitialized\n");
}

static long memflow_ioctl(struct file *filp, unsigned int cmd, unsigned long argp)
{
	void __user *user_args = (void *)argp;
	pid_t kernel_pid = 0;

	switch (cmd) {
		case MEMFLOW_OPEN_VM:
			if (copy_from_user(&kernel_pid, user_args, sizeof(pid_t)))
				return -EFAULT;
			break;
	}

	return -1;
}
