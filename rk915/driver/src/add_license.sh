#!/bin/bash

read_dir(){
    for file in `ls $1`
    do
        if [ -d $1"/"$file ]
        then
            read_dir $1"/"$file
        else
			file_name=$1"/"$file
            echo $file_name
			cat $file_name | grep "$license"
			if [ $? -eq 0 ]; then
				echo "skip $file_name"
			else
				sed -i "1i $license" $file_name
			fi
        fi
    done
}

license="# SPDX-License-Identifier: GPL-2.0"
read_dir $1
