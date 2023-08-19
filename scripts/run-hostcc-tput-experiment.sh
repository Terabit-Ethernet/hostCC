#!/bin/bash

help()
{
    echo "Usage: run-hostcc-tput-experiment
               [ -H | --home (home directory)]
               [ -S | --server (ip address of the server)]
               [ --server_intf (interface name for the server, for eg., ens2f0)]
               [ --num_servers (number of servers)]
               [ -C | --client (ip address of the client)]
               [ --client_intf (interface name for the client, for eg., ens2f0)]
               [ --num_clients (number of clients)]
               [ -E | --exp (experiment name, this name will be used to create output directories; default='hostcc-test')]
               [ -M | --MTU (default=4000) ]
               [ -d | --ddio (=0/1, whether DDIO is disabled or enabled) ]
               [ -c | --cpu_mask (comma separated CPU mask to run the app on, recommended to run on NUMA node local to the NIC for maximum performance; default=0) ]
               [ --mlc_cores (comma separated values for MLC cores, for eg., '1,2,3' for using cores 1,2 and 3. Use 'none' to skip running MLC.) ]
               [ --target_iio_thresh (hostCC specific parameter, target IIO occupancy threshold I_T; default = 70) ]
               [ --target_pcie_thresh (hostCC specific parameter, target PCIe threshold occupancy B_T; default = 84, which corresponds to 80Gbps app-level throughput) ]
               [ --target_pid (hostCC specific parameter, PID of the memory-intensive application process to which hostCC should perform process scheduling based host-local response, in case MBA response is less effective; by default we assume MLC is the app running and find its pid using pidof command) ]
               [ -h | --help  ]"
    exit 2
}

SHORT=H:,S:,C:,E:,M:,d:,c:,h
LONG=home:,server:,client:,num_servers:,num_clients:,server_intf:,client_intf:,exp:,txn:,MTU:,size:,ddio:,cpu_mask:,mlc_cores:,target_iio_thresh:,target_pcie_thresh:,help
OPTS=$(getopt -a -n run-hostcc-tput-experiment --options $SHORT --longoptions $LONG -- "$@")

VALID_ARGUMENTS=$# # Returns the count of arguments that are in short or long options

if [ "$VALID_ARGUMENTS" -eq 0 ]; then
  help
fi

eval set -- "$OPTS"

#default values
exp="hostcc-test"
server="192.168.10.122"
client="192.168.10.121"
server_intf="ens2f1"
client_intf="ens2f0"
num_servers=4
num_clients=4
init_port=3000
ddio=0
mtu=4000
dur=30
cpu_mask="4,8,12,16"
target_iio_thresh=70
target_pcie_thresh=84
mlc_cores="none"
mlc_dur=100
num_runs=2
home="/home/saksham"
hostcc_dir=$home/hostCC/src
setup_dir=$home/hostCC/utils
exp_dir=$home/hostCC/utils/tcp
mlc_dir=$home/mlc/Linux

echo -n "Enter SSH Username for client:"
read uname
echo -n "Enter SSH Address for client:"
read addr
echo -n "Enter SSH Password for client:"
read -s password
# uname=abc
# addr=xyz.com
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
    --mlc_cores )
      mlc_cores="$2"
      shift 2
      ;;
    --target_iio_thresh )
      target_iio_thresh="$2"
      shift 2
      ;;
    --target_pcie_thresh )
      target_pcie_thresh="$2"
      shift 2
      ;;
    --target_pid )
      target_pid="$2"
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
    sshpass -p $password ssh $uname@$addr 'screen -S $(screen -list | awk "/\\.client_session\t/ {print \$1}") -X quit'
    sshpass -p $password ssh $uname@$addr 'screen -S $(screen -list | awk "/\\.logging_session\t/ {print \$1}") -X quit'
    sshpass -p $password ssh $uname@$addr 'screen -wipe'
    sshpass -p $password ssh $uname@$addr 'sudo pkill -9 -f iperf'
}

function start_hostcc() {
    sudo bash ../utils/set_mba_levels.sh
    sudo dmesg --clear
    cd $hostcc_dir
    make
    if [ "$mlc_cores" = "none" ]; then
        sudo insmod hostcc-module.ko target_pcie_thresh=$target_pcie_thresh target_iio_thresh=$target_iio_thresh
    else
        target_pid=$(pidof mlc)
        echo "sudo insmod hostcc-module.ko target_pid=$target_pid target_pcie_thresh=$target_pcie_thresh target_iio_thresh=$target_iio_thresh"
        sudo insmod hostcc-module.ko target_pid=$target_pid target_pcie_thresh=$target_pcie_thresh target_iio_thresh=$target_iio_thresh
    fi
    sleep 10 #give time for hostcc module to load 
    cd -
}

function fin_hostcc() {
    local hostcc_log_dir=$1
    cd $hostcc_dir
    sudo rmmod hostcc-module.ko
    sleep 15
    sudo dmesg > $hostcc_log_dir/hcc.log
    cd -
    sudo bash ../utils/reset_mba_levels.sh
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

#### start hostCC
echo "Starting hCC..."
start_hostcc

#### setup and start servers
echo "setting up server config..."
cd $setup_dir
sudo bash setup-envir.sh -i $server_intf -a $server -m $mtu -d $ddio -f 1 -r 0 -p 0 -e 1 -b 1 -o 1
cd -

echo "starting server instances..."
cd $exp_dir
sudo bash run-netapp-tput.sh -m server -S $num_servers -o $exp-RUN-$j -p $init_port -c $cpu_mask &
sleep 2
cd -

#### setup and start clients
echo "setting up and starting clients..."
sshpass -p $password ssh $uname@$addr 'screen -dmS client_session sudo bash -c "cd '$setup_dir'; sudo bash setup-envir.sh -i '$client_intf' -a '$client' -m '$mtu' -d '$ddio' -f 1 -r 0 -p 0 -e 1 -b 1 -o 1; cd '$exp_dir'; sudo bash run-netapp-tput.sh -m client -a '$server' -C '$num_clients' -S '$num_servers' -o '$exp'-RUN-'$j' -p '$init_port' -c '$cpu_mask'; exec bash"'


#### warmup
echo "warming up..."
progress_bar 10 1

#record stats
##start sender side logging
echo "starting logging at client..."
sshpass -p $password ssh $uname@$addr 'screen -dmS logging_session sudo bash -c "cd '$setup_dir'; sudo bash record-host-metrics.sh -t 1 -i '$client_intf' -o '$exp-RUN-$j' --type 0 --cpu_util 1 --retx 1 --pcie 0 --membw 0 --dur '$dur' --cores '$cpu_mask' ; exec bash"'

##start receiver side logging
echo "starting logging at server..."
cd $setup_dir
sudo bash record-host-metrics.sh -t 1 -i $server_intf -o $exp-RUN-$j --type 0 --cpu_util 1 --pcie 1 --membw 1 --dur $dur --cores $cpu_mask
echo "done logging..."
cd -

#transfer sender-side info back to receiver
sshpass -p $password scp $uname@$addr:$setup_dir/reports/$exp-RUN-$j/retx.rpt $setup_dir/reports/$exp-RUN-$j/retx.rpt

sleep $(($dur * 2))

#post-run cleanup
fin_hostcc $setup_dir/logs/$exp-RUN-$j
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
    sudo bash setup-envir.sh -i $server_intf -a $server -m $mtu -d $ddio -f 1 -r 0 -p 0 -e 1 -b 1 -o 1
    cd -

    echo "starting server instances..."
    cd $exp_dir
    sudo bash run-netapp-tput.sh -m server -S $num_servers -o $exp-MLCRUN-$j -p $init_port -c $cpu_mask &
    sleep 2
    cd -

    #### setup and start clients
    echo "setting up and starting clients..."
    sshpass -p $password ssh $uname@$addr 'screen -dmS client_session sudo bash -c "cd '$setup_dir'; sudo bash setup-envir.sh -i '$client_intf' -a '$client' -m '$mtu' -d '$ddio' -f 1 -r 0 -p 0 -e 1 -b 1 -o 1; cd '$exp_dir'; sudo bash run-netapp-tput.sh -m client -a '$server' -C '$num_clients' -S '$num_servers' -o '$exp'-MLCRUN-'$j' -p '$init_port' -c '$cpu_mask'; exec bash"'

    #### start MLC
    echo "starting MLC..."
    $mlc_dir/mlc --loaded_latency -T -d0 -e -k$mlc_cores -j0 -b1g -t$mlc_dur -W2 &> $setup_dir/reports/$exp-MLCRUN-$j/mlc.log &

    #### start hostCC
    sleep 5  #give a few seconds for MLC to run, so that hostCC can register the MLC PID
    start_hostcc
    ## wait until MLC starts sending memory traffic at full rate
    sleep 60
    echo "Running MLC..."
    progress_bar $mlc_dur 1

    #post-run cleanup
    fin_hostcc $setup_dir/logs/$exp-MLCRUN-$j
    cleanup
    done
fi

#collect info from all runs
if [ "$mlc_cores" = "none" ]; then
    sudo python3 collect-tput-stats.py $exp $num_runs 0
else
    sudo python3 collect-tput-stats.py $exp $num_runs 1
fi


