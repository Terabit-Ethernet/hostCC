#!/bin/bash

help()
{
    echo "Usage: run-netapp-tput [ -m | --mode (=client/server) ]
               [ -o | --outdir (output directory to store the application stats log; default='test')]
               [ -S | --num_servers (number of server instances)]
               [ -C | --num_clients (number of client instances, only use this option at client)]
               [ -p | --port (port number for the first connection) ]
               [ -a | --addr (ip address of the server, only use this option at client) ]
               [ -c | --cores (comma separated cpu core values to run the clients/servers at, for eg., cpu=4,8,12,16; if the number of clients/servers > the number of input cpu cores, the clients/servers will round-robin over the provided input cores; recommended to run on NUMA node local to the NIC for maximum performance) ]
               [ -h | --help  ]"
    exit 2
}

SHORT=m:,o:,S:,C:,p:,a:,c:h
LONG=mode:,outdir:,num_servers:,num_clients:,port:,addr:,cores:,help
OPTS=$(getopt -a -n run-netapp-tput --options $SHORT --longoptions $LONG -- "$@")

VALID_ARGUMENTS=$# # Returns the count of arguments that are in short or long options

if [ "$VALID_ARGUMENTS" -eq 0 ]; then
  help
fi

eval set -- "$OPTS"

#default values
mode="server"
outdir="tcptest"
addr="192.168.10.122"
cores="4,8,12,16"
num_servers=4
num_clients=4
port=3000

IFS=',' read -ra core_values <<< $cores


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
    -S | --num_servers )
      num_servers="$2"
      shift 2
      ;;
    -C | --num_clients )
      num_clients="$2"
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
    -c | --cores )
      cores="$2"
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
rm ../logs/$outdir/iperf.bw.log

function collect_stats() {
  echo "Collecting app throughput for TCP server..."
  echo "Avg_iperf_tput: " $(cat ../logs/$outdir/iperf.bw.log | grep "60.*-90.*" | awk  '{ sum += $7; n++ } END { if (n > 0) printf "%.3f", sum/1000; }') > ../reports/$outdir/iperf.bw.rpt
}

counter=0
if [ "$mode" = "server" ]
then
    sudo pkill -9 -f iperf #kill existing iperf servers/clients
    while [ $counter -lt $num_servers ]; do
        index=$(( counter % ${#core_values[@]} ))
        core=${core_values[index]}
        echo "Starting server $counter on core $core"
        taskset -c $core nice -n -20 iperf3 -s --port $(($port + $counter)) -i 30 -f m --logfile ../logs/$outdir/iperf.bw.log &
        ((counter++))
    done
    echo "waiting for few minutes before collecting stats..."
    sleep 120
    echo "collecting stats..."
    collect_stats
elif [ "$mode" = "client" ]
then
    sudo pkill -9 -f iperf #kill existing iperf servers/clients
    while [ $counter -lt $num_clients ]; do
        index=$(( counter % ${#core_values[@]} ))
        core=${core_values[index]}
        echo "Starting client $counter on core $core"
        taskset -c $core nice -n -20 iperf3 -c $addr --port $(($port+$(($counter%$num_servers)))) -t 10000 -C dctcp &
        ((counter++))
    done
else
    echo "incorrect argument specified"
    help
fi