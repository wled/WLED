#!/bin/bash
FWPATH=/path/to/your/WLED/build_output/firmware

update_one() {
if [ -f $FWPATH/$2 ]; then
	ping -c 1 $1 >/dev/null
	PINGRESULT=$?
	if [ $PINGRESULT -eq 0 ]; then
		echo Updating $1
		curl -s -F "update=@${FWPATH}/$2" $1/update >/dev/null
		return 0
	fi
	return 1
fi
}

update_one 192.168.x.x firmware.bin
update_one 192.168.x.x firmware.bin
# ...