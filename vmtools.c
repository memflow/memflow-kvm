#include "vmtools.h"
#include "main.h"
#include "ksyms.h"
#include "mabi.h"
#include <linux/kvm_host.h>
#include <linux/anon_inodes.h>
#include <linux/sort.h>
#include <linux/mm.h>
#include <linux/mman.h>

static int memflow_vm_release(struct inode *inode, struct file *filp);
static long memflow_vm_ioctl(struct file *filp, unsigned int cmd, unsigned long argp);

static const struct file_operations memflow_vm_fops = {
	.release = memflow_vm_release,
	.unlocked_ioctl = memflow_vm_ioctl,
	.owner = THIS_MODULE
};

static int memflow_vm_mapped_release(struct inode *inode, struct file *filp);

struct vm_vma_map {
	unsigned long start, end;
};

struct vm_mapped_data {
	u32 mapped_vma_count;
	struct vm_vma_map vma_maps[KVM_MEM_SLOTS_NUM];
	//vm_map_info_t has flexible array at the end, _map_slots is the array.
	vm_map_info_t vm_map_info;
	struct vm_memslot _map_slots[KVM_MEM_SLOTS_NUM];
};

static const struct file_operations memflow_vm_mapped_fops = {
	.release = memflow_vm_mapped_release,
	.owner = THIS_MODULE
};

KSYMDEC(kvm_lock);
KSYMDEC(vm_list);

static int open_kvm(struct kvm *kvm)
{
	int fd = -1;

	fd = anon_inode_getfd("memflow-vm", &memflow_vm_fops, kvm, O_RDWR | O_CLOEXEC);

	if (fd < 0) {
		goto fail_fd;
	}

	return fd;

fail_fd:
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
	struct kvm *kvm = filp->private_data;
	kvm_put_kvm(kvm);
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

	//TODO: coalesce nearby slots

	return slot_count;
}

static int get_vm_info(struct kvm *kvm, vm_info_t __user *user_info)
{
	vm_info_t kernel_info;
	vm_memslot_t *memslot_map;
	int ret;
	u32 slot_count;

	ret = -1;

	if (copy_from_user(&kernel_info, user_info, sizeof(vm_info_t)))
		goto do_return;

	mutex_lock(&kvm->lock);
	mutex_lock(&kvm->slots_lock);

	if (!kernel_info.slot_count)
		goto unlock_kvm;

	// Clamp user provided sizes...
	if (kernel_info.slot_count > kvm_memslots(kvm)->used_slots)
		kernel_info.slot_count = kvm_memslots(kvm)->used_slots;

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

// Called with otask->mm->mm_sem locked for writing. Does not release the lock
static void remap_vmas(struct vm_mapped_data *data, struct task_struct *otask)
{
	int i, o, u;
	vm_memslot_t *slot;
	struct vm_vma_map *mapped_vma;
	struct vm_area_struct *vma;
	unsigned long retaddr;
	unsigned long offset;
	int map_flags;

	for (i = 0; i < data->mapped_vma_count; i++) {
		mapped_vma = data->vma_maps + i;

		vma = find_vma(otask->mm, mapped_vma->start);

		// Anonymous VMAs backed by nothing are not mappable
		if (!vma || !vma->vm_ops || !vma->vm_file || (void *)vma->vm_file == (void *)vma->vm_ops) {
			if (!vma) {
				mprintk("critical bug - no vma\n");
				goto remove_vma_map_entry;
			}

			mprintk("%d (%p %lx %lx) not mappable!\n", i, vma, vma->vm_start, vma->vm_end);

			goto remove_unmapped_slots;
		}

		mprintk("%d (%p %lx %lx %p %p %lx) %lx\n", i, vma, vma->vm_start, vma->vm_end, vma->vm_file, vma->vm_ops, vma->vm_flags, atomic_long_read(&vma->vm_file->f_count));

		vma->vm_flags |= VM_SHARED | VM_MAYSHARE;
		map_flags = MAP_SHARED;

		if (vma->vm_flags & VM_HUGETLB)
			map_flags |= MAP_HUGETLB;

		// In reality, this will try to allocate CoW memory. We need to somehow not do that
		retaddr = vm_mmap(vma->vm_file, 0, vma->vm_end - vma->vm_start, PROT_READ, map_flags, vma->vm_pgoff >> PAGE_SHIFT);

		if (retaddr > ~0ul - 0x100) {
			mprintk("ERR %ld\n", -retaddr);
			goto remove_unmapped_slots;
		}

		// Update the internal structure to point to the current userspace

		offset = retaddr - mapped_vma->start;

		for (o = 0; o < data->vm_map_info.slot_count; o++) {
			slot = data->vm_map_info.slots + o;
			if (slot->host_base >= mapped_vma->start && slot->host_base < mapped_vma->end) {
				slot->host_base += offset;
			}
		}

		mapped_vma->start += retaddr;
		mapped_vma->end += offset;

		continue;

remove_unmapped_slots:
		// Remove all memslots that correspond to this vma
		for (o = data->vm_map_info.slot_count - 1; o >= 0; o--) {
			slot = data->vm_map_info.slots + o;
			if (slot->host_base >= vma->vm_start && slot->host_base < vma->vm_end) {
				data->vm_map_info.slot_count--;
				for (u = o; u < data->vm_map_info.slot_count; u++) {
					data->vm_map_info.slots[u] = data->vm_map_info.slots[u + 1];
				}
			}
		}

remove_vma_map_entry:
		//Do swap remove
		data->vma_maps[i--] = data->vma_maps[--data->mapped_vma_count];
	}

	/*mprintk("Retained vmas:\n");

	for (i = 0; i < data->mapped_vma_count; i++) {
		mprintk("%d %lx-%lx\n", i, data->vma_maps[i].start, data->vma_maps[i].end);
	}

	mprintk("Retained memslots:\n");

	for (i = 0; i < data->vm_map_info.slot_count; i++) {
		slot = data->vm_map_info.slots + i;
		mprintk("%d %llx %llx %llx\n", i, slot->base, slot->host_base, slot->map_size);
	}*/
}

static void find_unique_vmas(struct vm_mapped_data *data, struct mm_struct *other_mm)
{
	int i, o;
	struct vm_area_struct *vma;
	vm_memslot_t *slot;

	for (i = 0; i < data->vm_map_info.slot_count; i++) {
		slot = data->vm_map_info.slots + i;
		vma = find_vma(other_mm, slot->host_base);

		for (o = 0; o < data->mapped_vma_count; o++) {
			if (vma->vm_start == data->vma_maps[o].start)
				goto skip_slot;
		}

		data->vma_maps[data->mapped_vma_count++] = (struct vm_vma_map) {
			.start = vma->vm_start,
			.end = vma->vm_end
		};

skip_slot:
		continue;
	}
}

static int do_map_vm(struct kvm *kvm, vm_map_info_t __user *user_info)
{
	struct vm_mapped_data *priv;
	int fd = -1, memslot_count;
	struct file *file;
	struct task_struct *other_task;
	struct mm_struct *other_mm;

	other_task = pid_task(find_vpid(kvm->userspace_pid), PIDTYPE_PID);

	// We could support doing the remapping in current process, but it's pointless and adds extra lock complexity
	if (!other_task || other_task == current)
		goto do_return;

	other_mm = other_task->mm;

	if (!other_mm || other_mm == current->mm)
		goto do_return;

	priv = vmalloc(sizeof(*priv));

	if (!priv)
		goto do_return;

	priv->mapped_vma_count = 0;
	priv->vm_map_info.slot_count = 0;

	//Do a copy to user now as well. If it fails to write here, ptr is RO, and we will avoid unmapping data 
	if (copy_from_user(&priv->vm_map_info, user_info, sizeof(vm_map_info_t)))
		goto free_alloc;

	if (!priv->vm_map_info.slot_count)
		goto free_alloc;

	if (priv->vm_map_info.slot_count > KVM_MEM_SLOTS_NUM)
		priv->vm_map_info.slot_count = KVM_MEM_SLOTS_NUM;

	fd = get_unused_fd_flags(O_CLOEXEC);

	if (fd < 0)
		goto free_alloc;

	mutex_lock(&kvm->lock);
	mutex_lock(&kvm->slots_lock);

	if ((memslot_count = get_sorted_memslots(kvm_memslots(kvm), priv->vm_map_info.slot_count, priv->vm_map_info.slots)) == -1)
		goto unlock_kvm;

	priv->vm_map_info.slot_count = memslot_count;

	down_write(&other_mm->mmap_sem);

	// First order of business is to grab all unique mappings to memslots (that are backed by some kind of file)
	find_unique_vmas(priv, other_mm);

	file = anon_inode_getfile("memflow-vm-map", &memflow_vm_mapped_fops, priv, O_RDWR);

	if (IS_ERR(file))
		goto unlock_mem;

	// Now remap all unique mappings
	remap_vmas(priv, other_task);

	if (!priv->mapped_vma_count)
		goto release_file;

	if (copy_to_user(user_info, &priv->vm_map_info, sizeof(vm_map_info_t) + sizeof(vm_memslot_t) * priv->vm_map_info.slot_count))
		goto release_file;

	fd_install(fd, file);

	up_write(&other_mm->mmap_sem);
	mutex_unlock(&kvm->lock);
	mutex_unlock(&kvm->slots_lock);

	return fd;

release_file:
	// The data will be freed later on, so we do not have to do that ourselves
	priv = NULL;
	fput(file);
unlock_mem:
	up_write(&other_mm->mmap_sem);
unlock_kvm:
	mutex_unlock(&kvm->lock);
	mutex_unlock(&kvm->slots_lock);
	put_unused_fd(fd);
free_alloc:
	if (priv)
		vfree(priv);
do_return:
	return -1;
}

static long memflow_vm_ioctl(struct file *filp, unsigned int cmd, unsigned long argp)
{
	switch (cmd) {
		case MEMFLOW_VM_INFO:
			return get_vm_info(filp->private_data, (vm_info_t __user *)argp);
		case MEMFLOW_MAP_VM:
			return do_map_vm(filp->private_data, (vm_map_info_t __user *)argp);
	}

	return -1;
}

static int memflow_vm_mapped_release(struct inode *inode, struct file *filp)
{
	vfree(filp->private_data);
	return 0;
}

