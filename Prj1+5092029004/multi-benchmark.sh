#!/bin/bash

make
rm -f multibench/*.out
mkdir -p multibench
echo NThread,Size,real,user,sys

for NThread in `seq 1 6`;
do
	for Size in `seq 128 128 1024`;
	do
		echo -n $NThread
		echo -n ,
		echo -n $Size
		echo -n ,
		/usr/bin/time -f "%e,%U,%S" ./multi $Size multibench/$NThread-$Size.out $NThread
	done
done

