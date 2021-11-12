#!/bin/bash

# -e  Exit immediately if a command exits with a non-zero status.
set -e

SCRIPT_NAME=${0##*/}

DEFCONFIG_PREFIX="imx6ul-var-dart-"
DEFCONFIG_SUFFIX="_defconfig"
DEFCONFIG_ORIG_SUFFIX=".orig"
PARAM_DEVICE_TYPE="twonav-2018"

### usage ###
function usage() {
    echo "Usage:"
	echo " ./${SCRIPT_NAME}"
}

###### parse input arguments ##
readonly SHORTOPTS="h:t:"
readonly LONGOPTS="help,type:"

ARGS=$(getopt -s bash --options ${SHORTOPTS}  \
  --longoptions ${LONGOPTS} --name ${SCRIPT_NAME} -- "$@" )

eval set -- "$ARGS"

while true; do
	case $1 in		
		-h|--help ) # get help
			usage
			exit 0;
			;;
		-- )
			shift
			break
			;;
		* )
			shift
			break
			;;
	esac
	shift
done

readonly DEFCONFIG="$DEFCONFIG_PREFIX$PARAM_DEVICE_TYPE$DEFCONFIG_SUFFIX"

# Clean
make ARCH=arm mrproper
# Choose defconfig
make ARCH=arm $DEFCONFIG
# Modify defconfig
make ARCH=arm xconfig QT_X11_NO_MITSHM=1
make ARCH=arm savedefconfig
# Backup original defconfig
cp arch/arm/configs/$DEFCONFIG arch/arm/configs/$DEFCONFIG$DEFCONFIG_ORIG_SUFFIX
# Apply changes
cp defconfig arch/arm/configs/$DEFCONFIG
# Delete current config
rm -rf defconfig

