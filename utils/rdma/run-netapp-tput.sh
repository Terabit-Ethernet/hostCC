#!/bin/bash

help()
{
    echo "Usage: run-netapp-tput [ -m | --mode (=client/server) ]
               [ -d | --dev (RDMA device, for eg., mlx5_0/mlx5_1 for port0/1 if using Mellanox CX5 NICs; device list be found at /sys/class/infiniband/) ]
               [ -a | --addr (ip address of the server, only use this option at client) ]
               [ -t | --txn (=read/write/send; provides what RDMA transaction to use) ]
               [ -M | --MTU (=1024/2048/4096; MTU size used) ]
               [ -s | --size (message size used in bytes, for eg., 65536 for 64KB message size) ]
               [ -x | --gid_index (=0/1 for RoCE v1, v2) ]
               [ -D | --dur (test duration in seconds) ]
               [ -S | --sl (service level (0-7), note that PFC will be configured per-service level) ]
               [ -c | --cpu (CPU to run the app on, recommended to run on NUMA node local to the NIC for maximum performance) ]
               [ -h | --help  ]"
    exit 2
}

SHORT=m:,d:,a:,t:,M:,s:,x:,D:,S:,h
LONG=mode:,dev:,addr:,txn:,MTU:,size:,gid_index:,dur:,sl:,help
OPTS=$(getopt -a -n run-netapp-tput --options $SHORT --longoptions $LONG -- "$@")

VALID_ARGUMENTS=$# # Returns the count of arguments that are in short or long options

if [ "$VALID_ARGUMENTS" -eq 0 ]; then
  help
fi

eval set -- "$OPTS"

#default values
mode="client"
dev="mlx5_1"
addr="192.168.11.122"
txn="write"
size=65536
mtu=4096
gid_index=1
dur=30
sl=0
cpu=0


while :
do
  case "$1" in
    -m | --mode )
      mode="$2"
      shift 2
      ;;
    -d | --dev )
      dev="$2"
      shift 2
      ;;
    -a | --addr )
      addr="$2"
      shift 2
      ;;
    -t | --txn )
      txn="$2"
      shift 2
      ;;
    -M | --MTU )
      mtu="$2"
      shift 2
      ;;
    -s | --size )
      size="$2"
      shift 2
      ;;
    -x | --gid_index )
      gid_index="$2"
      shift 2
      ;;
    -D | --dur )
      dur="$2"
      shift 2
      ;;
    -S | --sl )
      sl="$2"
      shift 2
      ;;
    -c | --cpu )
      cpu="$2"
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

if [ "$mode" = "server" ]
then
    if [ "$txn" = "read" ]
    then
        echo "taskset -c $cpu ib_read_bw -d $dev -x $gid_index --cpu_util --report_gbits -s $size -m $mtu -S $sl"
        taskset -c $cpu ib_read_bw -d $dev -x $gid_index --cpu_util --report_gbits -s $size -m $mtu -S $sl
    elif [ "$txn" = "write" ]
    then
        echo "taskset -c $cpu ib_write_bw -d $dev -x $gid_index --cpu_util --report_gbits -s $size -m $mtu -S $sl"
        taskset -c $cpu ib_write_bw -d $dev -x $gid_index --cpu_util --report_gbits -s $size -m $mtu -S $sl
    elif [ "$txn" = "send" ]
    then
        echo "taskset -c $cpu ib_send_bw -d $dev -x $gid_index --cpu_util --report_gbits -s $size -m $mtu -S $sl"
        taskset -c $cpu ib_send_bw -d $dev -x $gid_index --cpu_util --report_gbits -s $size -m $mtu -S $sl
    else
        echo "incorrect argument specified"
        help
    fi
elif [ "$mode" = "client" ]
then
    if [ "$txn" = "read" ]
    then
        echo "taskset -c $cpu ib_read_bw $addr -F -d $dev -x $gid_index --cpu_util --report_gbits -s $size -m $mtu -S $sl -D$dur"
        taskset -c $cpu ib_read_bw $addr -F -d $dev -x $gid_index --cpu_util --report_gbits -s $size -m $mtu -S $sl -D$dur
    elif [ "$txn" = "write" ]
    then
        echo "taskset -c $cpu ib_write_bw $addr -F -d $dev -x $gid_index --cpu_util --report_gbits -s $size -m $mtu -S $sl -D$dur"
        taskset -c $cpu ib_write_bw $addr -F -d $dev -x $gid_index --cpu_util --report_gbits -s $size -m $mtu -S $sl -D$dur
    elif [ "$txn" = "send" ]
    then
        echo "taskset -c $cpu ib_send_bw $addr -F -d $dev -x $gid_index --cpu_util --report_gbits -s $size -m $mtu -S $sl -D$dur"
        taskset -c $cpu ib_send_bw $addr -F -d $dev -x $gid_index --cpu_util --report_gbits -s $size -m $mtu -S $sl -D$dur
    else
        echo "incorrect argument specified"
        help
    fi
else
    echo "incorrect argument specified"
    help
fi