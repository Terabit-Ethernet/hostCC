#!/bin/bash

help()
{
    echo "Usage: run-rdma-tput-experiment
               [ -H | --home (home directory)]
               [ -S | --server (ip address of the server)]
               [ --server_dev (RDMA device for the server, for eg., mlx5_0/mlx5_1, depends upon the interface port number and the driver)]
               [ --server_intf (interface name for the server, for eg., ens2f0)]
               [ -C | --client (ip address of the client)]
               [ --client_dev (RDMA device for the client, for eg., mlx5_0/mlx5_1, depends upon the interface port number and the driver)]
               [ --client_intf (interface name for the client, for eg., ens2f0)]
               [ -E | --exp (experiment name, this name will be used to create output directories; default='rdma-test')]
               [ -t | --txn (=read/write/send; provides what RDMA transaction to use; default='write') ]
               [ -M | --MTU (=256/512/1024/2048/4096; MTU size used;default=4096) ]
               [ -s | --size (message size used in bytes, for eg., 65536 for 64KB message size; default=65536) ]
               [ -d | --ddio (=0/1, whether DDIO is disabled or enabled) ]
               [ -c | --cpu (CPU to run the app on, recommended to run on NUMA node local to the NIC for maximum performance; default=0) ]
               [ --mlc_cores (comma separated values for MLC cores, for eg., '4,8,12' for using cores 4,8 and 12. Use 'none' to skip running MLC.) ]
               [ -h | --help  ]"
    exit 2
}

SHORT=H:,S:,C:,E:,t:,M:,s:,d:,c:,h
LONG=home:,server:,client:,server-dev:,client-dev:,server-intf:,client-intf:,exp:,txn:,MTU:,size:,ddio:,cpu:,mlc_cores:,help
OPTS=$(getopt -a -n run-rdma-tput-experiment --options $SHORT --longoptions $LONG -- "$@")

VALID_ARGUMENTS=$# # Returns the count of arguments that are in short or long options

if [ "$VALID_ARGUMENTS" -eq 0 ]; then
  help
fi

eval set -- "$OPTS"

#default values
exp="rdma-test"
server="192.168.11.122"
client="192.168.11.121"
server_dev="mlx5_0"
client_dev="mlx5_1"
server_intf="ens2f0"
client_intf="ens2f1"
txn="write"
size=65536
ddio=0
mtu=4096
dur=30
sl=0
cpu=0
mlc_cores="none"
mlc_dur=100
num_runs=2
home="/home/saksham"
setup_dir=$home/hostCC/utils
exp_dir=$home/hostCC/utils/rdma
mlc_dir=$home/mlc/Linux

echo -n "Enter SSH Username for client:"
read uname
echo -n "Enter SSH Address for client:"
read addr
echo -n "Enter SSH Password for client:"
read -s password
# uname=abc
# addr=xyz.edu
# password=****


while :
do
  case "$1" in
    -H | --home )
      home="$2"
      shift 2
      ;;
    -E | --exp )
      exp="$2"
      shift 2
      ;;
    -S | --server )
      server="$2"
      shift 2
      ;;
    --server_dev )
      server_dev="$2"
      shift 2
      ;;
     --server_intf )
      server_intf="$2"
      shift 2
      ;;
    -C | --client )
      client="$2"
      shift 2
      ;;
    --client_dev )
      client_dev="$2"
      shift 2
      ;;
     --client_intf )
      client_intf="$2"
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
    -d | --ddio )
      ddio="$2"
      shift 2
      ;;
    -c | --cpu )
      cpu="$2"
      shift 2
      ;;
    --mlc_cores )
      mlc_cores="$2"
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


# Function to display the progress bar
function progress_bar() {
    local duration=$1
    local interval=$2

    for ((i = 0; i <= duration; i += interval)); do
        local progress=$((i * 100 / duration))
        local bar_length=$((progress / 2))
        local bar=$(printf "%-${bar_length}s" "=")
        printf "[%-${bar_length}s] %d%%" "$bar" "$progress"
        sleep "$interval"
        printf "\r"
    done
    printf "[%-${duration}s] %d%%\n" "$(printf "%-${duration}s" "=")" "100"
}

function cleanup() {
    sudo pkill -9 -f loaded_latency
    sudo pkill -9 -f ib_read_bw
    sudo pkill -9 -f ib_write_bw
    sudo pkill -9 -f ib_send_bw
    sshpass -p $password ssh $uname@$addr "screen -ls | awk '/\(Detached\)/ { system(\"screen -S \" \$1 \" -X quit\") }'"
    sshpass -p $password ssh $uname@$addr 'screen -wipe'
    sshpass -p $password ssh $uname@$addr 'sudo pkill -9 -f ib_read_bw'
    sshpass -p $password ssh $uname@$addr 'sudo pkill -9 -f ib_write_bw'
    sshpass -p $password ssh $uname@$addr 'sudo pkill -9 -f ib_send_bw'
}


#first run very long running MLC app, to find out network tput
for ((j = 0; j < $num_runs; j += 1)); do
echo "running instance $j"

#### pre-run cleanup -- kill any existing clients/screen sessions
cleanup

#### start MLC
if [ "$mlc_cores" = "none" ]; then
    echo "No MLC instance used..."
    # Perform actions for "none" input
else
    echo "starting MLC..."
    $mlc_dir/mlc --loaded_latency -T -d0 -e -k$mlc_cores -j0 -b1g -t10000 -W5 &> mlc.log &
    ## wait until MLC starts sending memory traffic at full rate
    echo "waiting for MLC for start..."
    progress_bar 30 1
fi

#### setup and start servers
echo "setting up server config..."
cd $setup_dir
sudo bash setup-envir.sh -i $server_intf -a $server -m $mtu -d $ddio -f 1 -r 1 -p 1
cd -

echo "starting server instances..."
cd $exp_dir
sudo bash run-netapp-tput.sh -m server -d $server_dev -t $txn -D $(($dur * 5)) -o $exp-RUN-$j -s $size -M $mtu &
sleep 2
cd -

#### setup and start clients
echo "setting up and starting clients..."
sshpass -p $password ssh $uname@$addr 'screen -dmS client_session sudo bash -c "cd '$setup_dir'; sudo bash setup-envir.sh -i '$client_intf' -a '$client' -m '$mtu' -d '$ddio' -f 1 -r 1 -p 1; cd '$exp_dir'; sudo bash run-netapp-tput.sh -m client -a '$server' -d '$client_dev' -t '$txn' -D '$(($dur * 5))' -o '$exp'-RUN-'$j' -s '$size' -M '$mtu'; exec bash"'


#### warmup
echo "warming up..."
progress_bar 10 1

#record stats
##start receiver side logging
echo "starting logging at server..."
cd $setup_dir
sudo bash record-host-metrics.sh -t 1 -i $server_intf -o $exp-RUN-$j --bw 1 --cpu_util 1 --pcie 1 --membw 1 --iio 1 --pfc 1 --dur $dur
echo "done logging..."
cd -

sleep $(($dur * 3))

#post-run cleanup
cleanup
done

if [ "$mlc_cores" = "none" ]; then
    echo "No MLC instance used... Skipping MLC throughput collection"
else
    #now run very long network app, to find out MLC tput
    for ((j = 0; j < $num_runs; j += 1)); do
    echo "running instance $j"

    #### pre-run cleanup -- kill any existing clients/screen sessions
    cleanup

    #### setup and start servers
    echo "setting up server config..."
    cd $setup_dir
    sudo bash setup-envir.sh -i $server_intf -a $server -m $mtu -d $ddio -f 1 -r 1 -p 1
    cd -

    echo "starting server instances..."
    cd $exp_dir
    sudo bash run-netapp-tput.sh -m server -d $server_dev -t $txn -D $(($dur * 5)) -o $exp-MLCRUN-$j -s $size -M $mtu &
    sleep 2
    cd -

    #### setup and start clients
    echo "setting up and starting clients..."
    sshpass -p $password ssh $uname@$addr 'screen -dmS client_session sudo bash -c "cd '$setup_dir'; sudo bash setup-envir.sh -i '$client_intf' -a '$client' -m '$mtu' -d '$ddio' -f 1 -r 1 -p 1; cd '$exp_dir'; sudo bash run-netapp-tput.sh -m client -a '$server' -d '$client_dev' -t '$txn' -D '$(($dur * 5))' -o '$exp'-MLCRUN-'$j' -s '$size' -M '$mtu'; exec bash"'

    #### start MLC
    echo "starting MLC..."
    $mlc_dir/mlc --loaded_latency -T -d0 -e -k$mlc_cores -j0 -b1g -t$mlc_dur -W5 &> $setup_dir/reports/$exp-MLCRUN-$j/mlc.log &
    ## wait until MLC starts sending memory traffic at full rate
    sleep 30
    echo "Running MLC..."
    progress_bar $mlc_dur 1

    #post-run cleanup
    cleanup
    done
fi

#collect info from all runs
if [ "$mlc_cores" = "none" ]; then
    sudo python3 collect-rdma-tput-stats.py $exp $num_runs 0
else
    sudo python3 collect-rdma-tput-stats.py $exp $num_runs 1
fi


