## Introduction ##

This project is a porting of [yaffs2](http://www.yaffs.net) library on VxWorks, including drivers for Samsung K9K8G08U0A and K9F1G08U0A on Intel Xscale IXP425.

## Compile ##

1. Modify the path in `setenv-local.bat`
2. `make`

## Flash Driver ##

The flash driver in `src/nandflash` is not dedicated for VxWorks. It is a general implementation of required operation cycles mentioned in the chip datasheet. Please replace it with your own flash driver.

## VxWorks Interface ##

The interface for VxWorks is mainly located in `src/yaffs2/yaffs2Lib/yaffs2Lib.c`. It is works as a VxWorks device driver. An example usage of these API could be found in `src/yaffs2/yaffs2Lib/yaff2Util.c`.
