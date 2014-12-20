darwin-sdk
==========

Source repository for the Darwin ARM SDK.

To build and install this package on a debian/ubuntu/mint system run the following:

```
dpkg-buildpackage -us -cu
sudo dpkg -i ../darwin-sdk*.deb
```

To build and install this package on other linux distros (until we get proper package support):

`sudo make install`

