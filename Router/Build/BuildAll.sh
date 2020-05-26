#!/bin/sh

BUILDMODEL="BLUE"
#BUILDMODEL="RED"

if [ ${BUILDMODEL} = "BLUE" ]; then
	make TWE_CHIP_MODEL=JN5164 clean
	make TWE_CHIP_MODEL=JN5164 all -j 4
elif [ ${BUILDMODEL} = "RED" ]; then
	make TWELITE=RED clean
	make TWELITE=RED all
else
	echo "Error"
fi