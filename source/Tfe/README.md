# Network emulation

## Software using the network

See the [list](Software.md).

## Alternative implementations

The Windows version emulates **raw socket** networking using [libpcap](https://npcap.com), while in Linux the default is [libslirp](https://gitlab.freedesktop.org/slirp/libslirp).

### Virtual DNS

It is important to remember that the Uthernet II card can work **without** raw sockets and does not need to worry about the issues in this section (see https://github.com/a2retrosystems/uthernet2/wiki/Virtual-W5100-with-DNS).

### libpcap

https://npcap.com

Used in AppleWin and VICE. It creates a *bridged* network, where AppleWin operates directly on the LAN.

Pros:
- no setup required
- server ports are automatically opened

Cons:
- elevated priviledges are required in Linux (`cap_net_raw=ep`)
- an Ethernet interface is required (no support for WiFi)
- manual MAC configuration to run multiple emulators on the same LAN

### libslirp

https://gitlab.freedesktop.org/slirp/libslirp

This creates a sub LAN for the emulator, which requires NAT to access the rest of the network.

Default for AppleWin on Linux, available in QEMU and Bochs (see https://bochs.sourceforge.io/doc/docbook/user/using-slirp.html).

Pros:
- no setup required
- well maintained (used in QEMU)
- runs as normal user
- no limit to the number of emulators running at the same time

Cons:
- not integrated in the Windows build (suggestions?)
- requires port forwarding to open server ports

### tuntap

Available in VICE (see https://sourceforge.net/p/vice-emu/patches/278) and Bochs (see https://bochs.sourceforge.io/doc/docbook/user/config-tuntap.html).

Pros:
- runs as normal user
- supports any network

Cons:
- `root` required to set it up
- Linux only?
- one interface per emulator

It is unclear if port forwarding is necessary.

## Other emulators

[Altirra](https://www.virtualdub.org/altirra.html) and [Ample](https://github.com/ksherlock/ample) probably implement a solution very similar to `libslirp`.

[FUSE](https://sourceforge.net/p/fuse-emulator/fuse/ci/master/tree/peripherals/nic/) has a partial implementation which only supports TCP & UDP sockets.
