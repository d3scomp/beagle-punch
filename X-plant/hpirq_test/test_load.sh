#!/bin/sh

TMP_FILE=/tmp/test_load_file
echo > $TMP_FILE

COUNTER=0
while [ $COUNTER -lt 10000000 ]; do

	echo 'A' >> $TMP_FILE

	COUNTER=`expr $COUNTER + 1`

done


