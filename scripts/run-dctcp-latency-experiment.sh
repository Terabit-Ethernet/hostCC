#!/bin/bash

help()
{
    echo "Usage: run-rdma-lat-experiment
               [ -H | --home (home directory)]
               [ -S | --server (ip address of the server)]
               [ --server_intf (interface name for the server, for eg., ens2f0)]
               [ --num_servers (number of servers)]
               [ -C | --client (ip address of the client)]
               [ --client_intf (interface name for the client, for eg., ens2f0)]
               [ --num_clients (number of clients)]
               [ -E | --exp (experiment name, this name will be used to create output directories; default='rdma-test')]
               [ -M | --MTU (default=4000) ]
               [ -d | --ddio (=0/1, whether DDIO is disabled or enabled) ]
               [ -c | --cpu_mask (comma separated CPU mask to run the app on, recommended to run on NUMA node local to the NIC for maximum performance; default=0) ]
               [ -L | --lat_app_core (core to run the latency-sensitive app on, recommended to run on NUMA node local to the NIC for maximum performance; default=0) ]
               [ --mlc_cores (comma separated values for MLC cores, for eg., '1,2,3' for using cores 1,2 and 3. Use 'none' to skip running MLC.) ]
               [ -h | --help  ]"
    exit 2
}

SHORT=H:,S:,C:,E:,M:,d:,c:,L:,h
LONG=home:,server:,client:,num_servers:,num_clients:,server_intf:,client_intf:,exp:,txn:,MTU:,size:,ddio:,cpu_mask:,lat_app_core:,mlc_cores:,help
OPTS=$(getopt -a -n run-rdma-lat-experiment --options $SHORT --longoptions $LONG -- "$@")

VALID_ARGUMENTS=$# # Returns the count of arguments that are in short or long options

if [ "$VALID_ARGUMENTS" -eq 0 ]; then
  help
fi

eval set -- "$OPTS"

#default values
exp="dctcp-lat-test"
server="192.168.11.116"
client="192.168.11.117"
server_intf="ens2f1np1"
client_intf="ens2f1"
num_servers=5
num_clients=5
init_port=3000
ddio=0
mtu=4000
dur=20
cpu_mask="0,4,8,12,16"
lat_app_core=20
lat_app_port=5050
mlc_cores="none"
mlc_dur=100
ring_buffer=1024
num_runs=3
home="/home/benny"
setup_dir=$home/hostCC/utils
exp_dir=$home/hostCC/utils/tcp
mlc_dir=$home/mlc/Linux

# echo -n "Enter SSH Username for client:"
# read uname
# echo -n "Enter SSH Address for client:"
# read addr
# echo -n "Enter SSH Password for client:"
# read -s password
uname=benny
addr=192.168.11.117
ssh_hostname=genie04.cs.cornell.edu
password=benny


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
    --num_servers )
      num_servers="$2"
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
    --num_clients )
      num_clients="$2"
      shift 2
      ;;
     --client_intf )
      client_intf="$2"
      shift 2
      ;;
    -M | --MTU )
      mtu="$2"
      shift 2
      ;;
    -d | --ddio )
      ddio="$2"
      shift 2
      ;;
    -c | --cpu_mask )
      cpu_mask="$2"
      shift 2
      ;;
    -L | --lat_app_core )
      lat_app_core="$2"
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
    sudo pkill -9 -f iperf
    sudo pkill -9 -f netperf
    sshpass -p $password ssh $uname@$addr 'screen -S $(screen -list | awk "/\\.client_session\t/ {print \$1}") -X quit'
    sshpass -p $password ssh $uname@$addr 'screen -S $(screen -list | awk "/\\.logging_session\t/ {print \$1}") -X quit'
    sshpass -p $password ssh $uname@$addr 'screen -wipe'
    sshpass -p $password ssh $uname@$addr 'sudo pkill -9 -f iperf'
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
    $mlc_dir/mlc --loaded_latency -T -d0 -e -k$mlc_cores -j0 -b1g -t10000 -W2 &> mlc.log &
    ## wait until MLC starts sending memory traffic at full rate
    echo "waiting for MLC for start..."
    progress_bar 30 1
fi

#### setup and start servers
echo "setting up server config..."
cd $setup_dir
sudo bash setup-envir.sh -i $server_intf -a $server -m $mtu -d $ddio --ring_buffer $ring_buffer -f 1 -r 0 -p 0 -e 1 -b 1 -o 1
cd -

echo "starting server instances..."
cd $exp_dir
sudo bash run-netapp-tput.sh -m server -S $num_servers -o $exp-LATRUN-$j -p $init_port -c $cpu_mask &
sleep 2
cd -

#### setup and start clients, and netserver on the client machine
echo "setting up and starting clients..."
sshpass -p $password ssh $uname@$addr 'screen -dmS client_session sudo bash -c "cd '$setup_dir'; sudo bash setup-envir.sh -i '$client_intf' -a '$client' -m '$mtu' -d '$ddio' -f 1 -r 0 -p 0 -e 1 -b 1 -o 1; cd '$exp_dir'; sudo bash run-netapp-tput.sh -m client -a '$server' -C '$num_clients' -S '$num_servers' -o '$exp'-RUN-'$j' -p '$init_port' -c '$cpu_mask'; sudo bash run-netapp-lat.sh -m netserver -p '$lat_app_port' -c '$lat_app_core' ;exec bash"'


#### warmup
echo "warming up..."
progress_bar 10 1

### start netperf clients and logging
for k in {128,512,2048,8192,32768}
do
     cd $exp_dir
     echo "Dumping Netperf Stats for RPC size $k bytes..."
     sudo bash run-netapp-lat.sh -m netperf -s $k -o $exp-LATRUN-$j -d 10 -a $client -p $lat_app_port -c $lat_app_core
     cd -
done

#post-run cleanup
cleanup
done

#collect info from all runs
sudo python3 collect-lat-stats.py $exp $num_runs


