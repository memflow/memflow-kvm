# memflow's KVM connector and its driver

This is a connector for Kernel based virtual machines, by using a driver that maps all KVM pages into the memflow process (userspace -> userspace DMA).

`memflow-kmod` includes the kernel module that performs the operations.

`memflow-kvm-ioctl` provides a rust based IOCTL api to the kernel module.

`memflow-kvm` provides a memflow physical memory connector that uses the ioctl.

## Setting up

Install the connector using `./install.sh`. This will compile the connector in release mode and place it under `~/.local/lib/memflow/` directory, which can then be accessed by memflow clients. Do copy out the underlying shared library to `/usr/local/lib/memflow/` if you want to use it across all users.

## Setting up the module

Stable versions are available under [releases](https://github.com/memflow/memflow-kvm/releases).

Debian/Ubuntu package can be installed with `sudo dpkg -i memflow-dkms_${VERSION}_amd64.deb`, where `VERSION` is the version of the downloaded module.

For other distributions, run this command:

```
sudo dkms install --archive=memflow-${VERSION}-source-only.dkms.tar.gz
```

### Manually building the module

Run `make`. output will be placed in `build/memflow.ko`.

## Licensing note

While `memflow-kvm-ioctl`, and `memflow-kvm` are licensed under the `MIT` license, `memflow-kmod` is licensed only under `GPL-2`.
