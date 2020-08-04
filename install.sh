#!/bin/bash

cargo build --release --all-features
if [[ ! -d ~/.local/lib/memflow ]]; then
    mkdir ~/.local/lib/memflow
fi
cp target/release/libmemflow_kvm.so ~/.local/lib/memflow
