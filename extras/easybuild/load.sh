#!/bin/bash

zcat /proc/config.gz 2> /dev/null | grep CONFIG_PGFAULT_SAMPLE_PROFILER=y > /dev/null

if [ $? != 0 ]
then
	echo "KERNEL DOES NOT SUPPORT SHAMBLES"
else
	module use /home1/private/marios4/.local/easybuild/modules/all
	module load SHAMBLES/0.1
fi 
