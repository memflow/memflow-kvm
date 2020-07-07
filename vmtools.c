#include "vmtools.h"
#include "main.h"
#include "ksyms.h"
#include "mabi.h"
#include <linux/kvm_host.h>
#include <linux/anon_inodes.h>
#include <linux/sort.h>

static int memflow_vm_release(struct inode *inode, struct file *filp);
static long memflow_vm_ioctl(struct file *filp, unsigned int cmd, unsigned long argp);
static int memflow_vm_mmap(struct file *file, struct vm_area_struct *vma);

struct vm_private_data {
	struct kvm *kvm;
	int mapped;
	vm_map_info_t map_info;
};

static struct file_operations memflow_vm_fops = {
	.release = memflow_vm_release,
	.unlocked_ioctl = memflow_vm_ioctl,
	.mmap = memflow_vm_mmap
};

KSYMDEC(kvm_lock);
KSYMDEC(vm_list);

static int open_kvm(struct kvm *kvm)
{
	int fd = -1;
	struct vm_private_data *priv;

	priv = vmalloc(sizeof(struct vm_private_data));

	if (!priv)
		goto fail_alloc;

	priv->kvm = kvm;

	fd = anon_inode_getfd("memflow-vm", &memflow_vm_fops, priv, O_RDWR | O_CLOEXEC);

	if (fd < 0) {
		goto fail_fd;
	}

	return fd;

fail_fd:
	vfree(priv);
fail_alloc:
	kvm_put_kvm(kvm);

	return -1;
}

int open_vm(pid_t target_pid)
{
	struct kvm *kvm;
	int ret = -1;

	mutex_lock(_kvm_lock);

	list_for_each_entry(kvm, _vm_list, vm_list) {
		kvm_get_kvm(kvm);

		if (target_pid == 0 || target_pid == kvm->userspace_pid) {
			ret = open_kvm(kvm);
			break;
		}

		kvm_put_kvm(kvm);
	}

	mutex_unlock(_kvm_lock);

	return ret;
}

static int memflow_vm_release(struct inode *inode, struct file *filp)
{
	struct vm_private_data *priv = filp->private_data;
	kvm_put_kvm(priv->kvm);
	return 0;
}

static int memslot_compare(const void *lhs, const void *rhs) {
	gfn_t lbase = ((vm_memslot_t *)lhs)->base;
	gfn_t rbase = ((vm_memslot_t *)rhs)->base;

	if (lbase < rbase)
		return -1;
	if (lbase > rbase)
		return 1;
	return 0;
}

static int get_sorted_memslots(struct kvm_memslots *slots, int max_slots, vm_memslot_t *slots_out)
{
	struct kvm_memory_slot *slot;
	int slot_count, i;

	slot_count = 0;
	for (i = 0; i < KVM_MEM_SLOTS_NUM && slot_count < max_slots; i++) {
		slot = slots->memslots + i;
		if (slot->npages) {
			if (slot_count > slots->used_slots) {
				mprintk("Critical error: memory slot overflow\n");
				return -1;
			}

			slots_out[slot_count++] = (vm_memslot_t) {
				.base = gfn_to_gpa(slot->base_gfn),
				.host_base = slot->userspace_addr,
				.map_size = gfn_to_gpa(slot->npages)
			};
		}
	}

	sort(slots_out, slot_count, sizeof(*slots_out), memslot_compare, NULL);

	return slot_count;
}

static int get_vm_info(struct vm_private_data *data, vm_info_t __user *user_info)
{
	vm_info_t kernel_info;
	vm_memslot_t *memslot_map;
	int ret;
	u32 slot_count;
	struct kvm *kvm = data->kvm;

	ret = -1;

	if (copy_from_user(&kernel_info, user_info, sizeof(vm_info_t)))
		goto do_return;

	mutex_lock(&kvm->lock);
	mutex_lock(&kvm->slots_lock);

	// Supposedly exactly used_slots should have non-zero sized values, but what if not?
	memslot_map = vmalloc(sizeof(*memslot_map) * kernel_info.slot_count);

	if (!memslot_map)
		goto unlock_kvm;

	if ((slot_count = get_sorted_memslots(kvm_memslots(kvm), kernel_info.slot_count, memslot_map)) == -1)
		goto free_slots;

	kernel_info.userspace_pid = kvm->userspace_pid;
	kernel_info.slot_count = slot_count;

	if (copy_to_user(user_info, &kernel_info, sizeof(vm_info_t)))
		goto free_slots;
	if (copy_to_user(&user_info->slots, memslot_map, sizeof(*memslot_map) * slot_count))
		goto free_slots;

	ret = 0;

free_slots:
	vfree(memslot_map);
unlock_kvm:
	mutex_unlock(&kvm->slots_lock);
	mutex_unlock(&kvm->lock);
do_return:
	return ret;
}

static int get_vm_map_info(struct vm_private_data *data, vm_map_info_t __user *user_info)
{
	if (!data->mapped)
		return -1;

	return -1;
}

static long memflow_vm_ioctl(struct file *filp, unsigned int cmd, unsigned long argp)
{
	switch (cmd) {
		case MEMFLOW_VM_INFO:
			return get_vm_info(filp->private_data, (vm_info_t __user *)argp);
		case MEMFLOW_VM_MAP_INFO:
			return get_vm_map_info(filp->private_data, (vm_map_info_t __user *)argp);
	}

	return -1;
}

static int memflow_vm_mmap(struct file *file, struct vm_area_struct *vma)
{
	return -1;
}
