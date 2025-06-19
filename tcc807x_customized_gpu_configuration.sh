#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.
config_file=./.config
gpu=0
gpu_vz=0
subcore=0
DT_PATH="arch/arm64/boot/dts/telechips/tcc807x"

git checkout $DT_PATH/tcc807x-gpu.dtsi
git checkout $DT_PATH/tcc807x-subcore-gpu.dtsi
git checkout $DT_PATH/tcc8070-lpd4x322.dts
git checkout $DT_PATH/tcc8070-ivi-lpd4x322.dts
git checkout $DT_PATH/tcc8070-subcore-lpd4x322.dts
git checkout $DT_PATH/tcc8070-ivi-subcore-lpd4x322.dts

for line in `cat $config_file`
do
	if [[ "$line" == "CONFIG_MALI_MIDGARD=y" ]]; then
                gpu=1
        fi
        if [[ "$line" == "CONFIG_TCC_MALI_VZ=y" ]]; then
                gpu_vz=1
        fi
	if [[ "$line" == "CONFIG_TCC807X_CA55_SUB=y" ]]; then
		subcore=1
	fi
done

if [ $gpu_vz -eq 0 ]; then
	if [ $subcore -eq 0 ]; then
		cat $DT_PATH/tcc807x-gpu-nonvz.dtsi > $DT_PATH/tcc807x-gpu.dtsi	
		if [ $gpu -eq 0 ]; then
			sed -i '/tcc807x-gpu.dtsi/d' $DT_PATH/tcc8070-lpd4x322.dts
			sed -i '/tcc807x-gpu.dtsi/d' $DT_PATH/tcc8070-ivi-lpd4x322.dts
		fi
	else
		cat $DT_PATH/tcc807x-gpu-nonvz.dtsi > $DT_PATH/tcc807x-subcore-gpu.dtsi
		if [ $gpu -eq 0 ]; then
			sed -i '/tcc807x-subcore-gpu.dtsi/d' $DT_PATH/tcc8070-subcore-lpd4x322.dts
			sed -i '/tcc807x-subcore-gpu.dtsi/d' $DT_PATH/tcc8070-ivi-subcore-lpd4x322.dts
		fi		
	fi
else
	sed -n '1,/Subcore/p' $DT_PATH/tcc807x-gpu-vz.dtsi > $DT_PATH/tcc807x-gpu.dtsi
	sed -i '$ d' $DT_PATH/tcc807x-gpu.dtsi
	sed -n "/Subcore/,\$p" $DT_PATH/tcc807x-gpu-vz.dtsi > $DT_PATH/tcc807x-subcore-gpu.dtsi	
	sed -i '/tcc807x-gpu.dtsi/d' $DT_PATH/tcc8070-lpd4x322.dts
	if [ $subcore -eq 0 ]; then
		sed -i '/tcc807x-gpu.dtsi/d' $DT_PATH/tcc8070-lpd4x322.dts
		sed -i '/tcc807x-gpu.dtsi/d' $DT_PATH/tcc8070-ivi-lpd4x322.dts
	fi	
fi

