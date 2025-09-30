#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.
config_file=./.config
gpu_vz=0
subcore=0
DT_PATH="arch/arm64/boot/dts/telechips/tcc805x"

rm $DT_PATH/tcc805x-gpu.dtsi
rm $DT_PATH/tcc805x-subcore-gpu.dtsi
touch $DT_PATH/tcc805x-gpu.dtsi
touch $DT_PATH/tcc805x-subcore-gpu.dtsi

for line in `cat $config_file`
do
        if [[ "$line" == "CONFIG_POWERVR_VZ=y" ]]; then
                gpu_vz=1
        fi
	if [[ "$line" == "CONFIG_TCC805X_CA53Q=y" ]]; then
		subcore=1
	fi
done


if [ $gpu_vz -eq 0 ]; then
	if [ $subcore -eq 0 ]; then
		cat $DT_PATH/tcc805x-gpu-nonvz.dtsi > $DT_PATH/tcc805x-gpu.dtsi	
	else
		cat $DT_PATH/tcc805x-gpu-nonvz.dtsi > $DT_PATH/tcc805x-subcore-gpu.dtsi
	fi
else
	cat $DT_PATH/tcc805x-gpu-vz.dtsi > $DT_PATH/tcc805x-gpu.dtsi
	cat $DT_PATH/tcc805x-gpu-nonvz.dtsi > $DT_PATH/tcc805x-subcore-gpu.dtsi
fi

