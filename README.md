# memflow's KVM connector and its driver

This is a connector for Kernel based virtual machines, by using a driver that maps all KVM pages into the memflow process (userspace -> userspace DMA).

`memflow-kmod` includes the kernel module that performs the operations.

`memflow-kvm-ioctl` provides a rust based IOCTL api to the kernel module.

`memflow-kvm` provides a memflow physical memory connector that uses the ioctl.

## Setting up

Build the kernel module in `memflow-kmod`, output will be placed in `./build/memflow.ko` (relative to the git's repo root).

Install the connector using `./install.sh`. This will compile the connector in release mode and place it under `~/.local/lib/memflow/` directory, which can then be accessed by memflow clients. Do copy out the underlying shared library to `/usr/local/lib/memflow/` if you want to use it across all users.

## Licensing note

While `memflow-kvm-ioctl`, and `memflow-kvm` are licensed under the `MIT` license, `memflow-kmod` is licensed only under `GPL-2`.
