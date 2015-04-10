darwin-sdk
==========

Source repository for the Darwin ARM SDK.

This package requires the following to be installed before building:
 * dpkg-dev (if on debian/ubuntu/mint)
 * dh-scripts (if on debian/ubuntu/mint)
 * debhelper (if on debian/ubuntu/mint)
 * clang-3.4 (currently clang-3.5 breaks xnu build)
 * llvm-dev
 * uuid-dev
 * libssl-dev
 * libblocksruntime-dev
 * libc6-dev-i386 (if 64bit)
 * gcc-multilib (if 64bit)
 * build-essential
 * flex
 * tcsh
 * bison
 * automake
 * autogen
 * libtool
 * perl

NOTE:  Package names may vary from one distro to the next.

To build and install this package on a debian/ubuntu/mint system run the following:

```
dpkg-buildpackage -us -uc
sudo dpkg -i ../darwin-sdk*.deb
```

To build and install this package on other linux distros (until we get proper package support):

`sudo make install`
