#!/bin/bash

help()
{
    echo "Usage: run-netapp-lat [ -m | --mode (=netperf/netserver) ]
               [ -o | --outdir (output directory to store the application stats log; default='test')]
               [ -s | --size (size of request in bytes)]
               [ -p | --port (port number of netserver) ]
               [ -a | --addr (ip address of netserver, used only by netperf) ]
               [ -d | --dur (duration of run in seconds) ]
               [ -c | --core (core on which to run netserver/netperf at) ]
               [ -h | --help  ]"
    exit 2
}

SHORT=m:,o:,s:,p:,a:,d:,c:,h
LONG=mode:,outdir:,size:,port:,addr:,dur:,core:,help
OPTS=$(getopt -a -n run-netapp-lat --options $SHORT --longoptions $LONG -- "$@")

VALID_ARGUMENTS=$# # Returns the count of arguments that are in short or long options

if [ "$VALID_ARGUMENTS" -eq 0 ]; then
  help
fi

eval set -- "$OPTS"

#default values
mode="netperf"
outdir="tcplattest"
addr="192.168.10.121"
core=20
port=5050
size=128
dur=100


while :
do
  case "$1" in
    -m | --mode )
      mode="$2"
      shift 2
      ;;
    -o | --outdir )
      outdir="$2"
      shift 2
      ;;
    -p | --port )
      port="$2"
      shift 2
      ;;
    -a | --addr )
      addr="$2"
      shift 2
      ;;
    -d | --dur )
      dur="$2"
      shift 2
      ;;
    -c | --core )
      core="$2"
      shift 2
      ;;
    -h | --help)
      help
      ;;
    --)
      shift;
      break
      ;;
    *)
      echo "Unexpected option: $1"
      help
      ;;
  esac
done

mkdir -p ../reports #Directory to store collected logs
mkdir -p ../reports/$outdir #Directory to store collected logs
mkdir -p ../logs #Directory to store collected logs
mkdir -p ../logs/$outdir #Directory to store collected logs
rm netserver.log

counter=0
if [ "$mode" = "netserver" ]
then
    sudo pkill -9 -f netserver #kill existing netperf session
    taskset -c $core netserver -p $port -D f
elif [ "$mode" = "netperf" ]
then
    sudo pkill -9 -f netperf #kill existing netperf session
    taskset -c $core netperf -H $addr -t TCP_RR -l $dur -p $port -j MIN_LATENCY -f g -- -r $size,$size -o throughput > ../reports/$outdir/netperf.tput.rpt
    sleep 2
    cp netserver.log ../logs/$outdir/netperf.lat.log
    python3 ../print_netperf_latency_stats.py ../logs/$outdir/netperf.lat.log > ../reports/$outdir/netperf.lat.rpt
else
    echo "incorrect argument specified"
    help
fi