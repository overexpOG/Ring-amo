# Measure Resident Set size (RSS) and running time of process
# Shows also RSS of the process at the beginning of execution:
# this can be useful to exclude shared memory from allocated
# memory.
#
# Usage: monitor <pid>	
#
# You can execute and monitor a process with
# 
# your_program <args> & monitor.sh $! output
#

pid=$1
out=$2

rss_min=`ps -p $pid -o rss | tail -n 1`
rss_max=0

START=$(date +%s.%N)

while kill -0 $pid 2> /dev/null; do

	rss=`ps -p $pid -o rss | tail -n 1`

	echo $rss >> $out

	if ((rss > rss_max)) 
	then
		rss_max=$rss
	fi

	sleep .2
    
done

END=$(date +%s.%N)
DIFF=$(echo "$END - $START" | bc)

echo "Peak RSS = "$rss_max
echo "Start RSS = "$rss_min
echo "Difference RSS (Peak - Start) = "$((rss_max-rss_min))
echo "Total Running Time = "$DIFF