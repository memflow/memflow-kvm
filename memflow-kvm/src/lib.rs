use log::{debug, info};

use memflow_core::connector::{ConnectorArgs, MappedPhysicalMemory};
use memflow_core::mem::cloneable_slice::CloneableSliceMut;
use memflow_core::mem::MemoryMap;
use memflow_core::mem::PoolablePhysicalMemory;
use memflow_core::types::Address;
use memflow_core::{Error, Result};
use memflow_derive::connector;
use memflow_kvm_ioctl::{AutoMunmap, VMHandle};
use std::sync::Arc;

#[derive(Clone)]
struct KVMMapData<T: Clone> {
    handle: Arc<AutoMunmap>,
    mappings: MemoryMap<T>,
}

impl<T: Clone> AsRef<MemoryMap<T>> for KVMMapData<T> {
    fn as_ref(&self) -> &MemoryMap<T> {
        &self.mappings
    }
}

impl<'a> KVMMapData<CloneableSliceMut<'a, u8>> {
    unsafe fn from_addrmap_mut(handle: Arc<AutoMunmap>, map: MemoryMap<(Address, usize)>) -> Self {
        Self {
            handle,
            mappings: map.clone().into_cloneable_bufmap_mut(),
        }
    }
}

/// Creates a new KVM Connector instance.
#[connector(name = "kvm")]
pub fn create_connector(args: &ConnectorArgs) -> Result<impl PoolablePhysicalMemory> {
    let pid = match args.get_default() {
        Some(pidstr) => Some(
            pidstr
                .parse::<i32>()
                .map_err(|_| Error::Connector("Failed to parse PID"))?,
        ),
        None => None,
    };

    let vm = VMHandle::try_open(pid).map_err(|_| Error::Connector("Failed to get VM handle"))?;
    let (pid, memslots) = vm
        .info(64)
        .map_err(|_| Error::Connector("Failed to get VM info"))?;
    debug!("pid={} memslots.len()={}", pid, memslots.len());
    for slot in memslots.iter() {
        debug!(
            "{:x}-{:x} -> {:x}-{:x}",
            slot.base,
            slot.base + slot.map_size,
            slot.host_base,
            slot.host_base + slot.map_size
        );
    }
    let mapped_memslots = vm
        .map_vm(64)
        .map_err(|_| Error::Connector("Failed to map VM mem"))?;

    let mut mem_map = MemoryMap::new();

    info!("mmapped {} slots", mapped_memslots.len());
    for slot in mapped_memslots.iter() {
        debug!(
            "{:x}-{:x} -> {:x}-{:x}",
            slot.base,
            slot.base + slot.map_size,
            slot.host_base,
            slot.host_base + slot.map_size
        );
        mem_map.push_remap(
            slot.base.into(),
            slot.map_size as usize,
            slot.host_base.into(),
        );
    }

    let munmap = Arc::new(unsafe { AutoMunmap::new(mapped_memslots) });

    Ok(unsafe { MappedPhysicalMemory::with_info(KVMMapData::from_addrmap_mut(munmap, mem_map)) })
}
