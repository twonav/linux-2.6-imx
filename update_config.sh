make ARCH=arm mrproper
make ARCH=arm imx6ul-var-dart-twonav_defconfig
make ARCH=arm xconfig
make ARCH=arm savedefconfig
cp arch/arm/configs/imx6ul-var-dart-twonav_defconfig arch/arm/configs/imx6ul-var-dart-twonav_defconfig.orig
cp defconfig arch/arm/configs/imx6ul-var-dart-twonav_defconfig

