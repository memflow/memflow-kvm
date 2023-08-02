# memflow's KVM connector and its driver

This is a connector for Kernel based virtual machines, by using a driver that maps all KVM pages into the memflow process (userspace -> userspace DMA).

`memflow-kmod` includes the kernel module that performs the operations.

`memflow-kvm-ioctl` provides a rust based IOCTL api to the kernel module.

`memflow-kvm` provides a memflow physical memory connector that uses the ioctl.

## Setting up

#### Connector

Recommended way is to use [memflowup](https://github.com/memflow/memflowup).

#### Kernel module

**Your kernel must be compiled with `CONFIG_KALLSYMS=y`, and `CONFIG_KALLSYMS_ALL=y` being set in kconfig.**

Stable versions are available under [releases](https://github.com/memflow/memflow-kvm/releases).

Debian/Ubuntu package can be installed with `sudo dpkg -i memflow-dkms_${VERSION}_amd64.deb`, where `VERSION` is the version of the downloaded module.

For other distributions, run this command:

```
sudo dkms install --archive=memflow-${VERSION}-source-only.dkms.tar.gz
```

## Manually installing

#### Connector

Install the connector using `memflowup build --name memflow-kvm`. This will compile the connector in release mode and place it under `~/.local/lib/memflow/` directory, which can then be accessed by memflow clients. Do copy out the underlying shared library to `/usr/local/lib/memflow/` if you want to use it across all users.

#### Kernel module

Initialize submodules:

```
git submodule update --init
```

Run `make`. output will be placed in `build/memflow.ko`.

## FAQ

Q. I'm getting this warning:

```
warning: couldn't execute `llvm-config --prefix` (error: No such file or directory (os error 2))
warning: set the LLVM_CONFIG_PATH environment variable to the full path to a valid `llvm-config` executable (including the executable itself)
```

A. This warning is harmless and can be safely ignored.

## Licensing note

While `memflow-kvm-ioctl`, and `memflow-kvm` are licensed under the `MIT` license, `memflow-kmod` is licensed only under `GPL-2`.
