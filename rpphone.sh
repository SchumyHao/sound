#/bin/sh

while true
do
	STATUS=`gpio read 10`
        if [ $STATUS -eq 0 ]
	then
		sleep 1
		sound &
		while [ `gpio read 10` -eq 0 ]
		do
			sleep 1
		done
		killall sound
		killall aplay
		killall arecord
	fi
done
