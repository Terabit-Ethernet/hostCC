help()
{
    echo "Usage: setup-envir [ -H | --home (home directory)]
               [ -m | --mtu (MTU size in bytes; default=4000 for TCP, 4096 for RDMA) ] 
               [ -d | --ddio (=0/1, whether DDIO should be disabled/enabled; default=0) ]
               [ -i | --intf (interface name, eg. ens2f0) ]
               [ -a | --addr (ip address for the interface) ]
               [ -o | --opt (enable TCP optimization TSO,GRO,aRFS) ]
               [ -b | --buf (TCP socket buffer size (in MB)) ]
               [ -e | --ecn (Enable ECN support in Linux stack) ]
               [ -f | --hwpref (Enable hardware prefetching) ]
               [ -r | --rdma (=0/1, whether the setup is for running RDMA experiments (MTU offset will be different)) ]
               [ -p | --pfc (=0/1, disable or enable PFC) ]
               [ -h | --help  ]"
    exit 2
}

SHORT=H:,m:,d:,i:,a:,o:,b:,e:,f:,r:,p:,h
LONG=home:,mtu:,ddio:,intf:,addr:,opt:,buf:,ecn:,hwpref:,rdma:,pfc:,help
OPTS=$(getopt -a -n setup-envir --options $SHORT --longoptions $LONG -- "$@")

VALID_ARGUMENTS=$# # Returns the count of arguments that are in short or long options

if [ "$VALID_ARGUMENTS" -eq 0 ]; then
  help
fi

eval set -- "$OPTS"

#default values
home='/home/saksham'
mtu=4000
ddio=0
intf="ens2f1"
addr="192.168.10.122"
opt=1
buf=1
ecn=1
hwpref=1
rdma=0
pfc=0



while :
do
  case "$1" in
     -H | --home )
      home="$2"
      shift 2
      ;;
    -m | --mtu )
      mtu="$2"
      shift 2
      ;;
    -d | --ddio )
      ddio="$2"
      shift 2
      ;;
    -i | --intf )
      intf="$2"
      shift 2
      ;;
    -a | --addr )
      addr="$2"
      shift 2
      ;;
    -o | --opt )
      opt="$2"
      shift 2
      ;;
    -b | --buf )
      buf="$2"
      shift 2
      ;;
    -e | --ecn )
      ecn="$2"
      shift 2
      ;;
    -f | --hwpref )
      hwpref="$2"
      shift 2
      ;;
    -p | --pfc )
      pfc="$2"
      shift 2
      ;;
    -r | --rdma )
      rdma="$2"
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

if [ "$rdma" = 1 ]
then
 echo "Configuring MTU according to RDMA supported values..."
 mtu=$(($mtu + 96))
fi

# setup the interface
echo "Setting up the interface...$intf"
ifconfig $intf up
ifconfig $intf $addr
ifconfig $intf mtu $mtu

#disable TCP buffer auto-tuning, and set the buffer size to the specified size
echo "Setting up the socket buffer size to be ${buf}MB"
echo 0 > /proc/sys/net/ipv4/tcp_moderate_rcvbuf 
#Set TCP receive buffer size to be 1MB (other 1MB is for the application buffer)
echo "$(($buf * 2000000)) $(($buf * 2000000)) $(($buf * 2000000))" > /proc/sys/net/ipv4/tcp_rmem 
#Set TCP send buffer size to be 1MB
echo "$(($buf * 1000000)) $(($buf * 1000000)) $(($buf * 1000000))" > /proc/sys/net/ipv4/tcp_wmem

#Enable TCP ECN support at the senders/receivers
if [ "$ecn" = 1 ]
then
    echo "Enabling ECN support..."
    echo 1 > /proc/sys/net/ipv4/tcp_ecn
fi

#Enable aRFS, TSO, GRO for the interface
if [ "$opt" = 1 ]
then
    cd $home/terabit-network-stack-profiling/
    echo "Enabling TCP optimizations (TSO, GRO, aRFS)..."
    sudo python network_setup.py $intf --arfs --mtu $mtu --sock-size --tso --gro
    cd -
fi


#Enable/disable DDIO
cd $home/ddio-bench/
if [ "$ddio" = 1 ]; then
    echo "Enabling DDIO..."
    ./change-ddio-on
else
    echo "Disabling DDIO..."
    ./change-ddio-off
fi
cd -


#Enable prefetching
if [ "$hwpref" = 1 ]
then
    echo "Enabling hardware prefetching..."
    modprobe msr
    wrmsr -a 0x1a4 0
else
    echo "Disabling hardware prefetching..."
    modprobe msr
    wrmsr -a 0x1a4 1
fi

#Enable PFC (on QoS 0)
if [ "$pfc" = 1 ]
then
    echo "Enabling PFC..."
    mlnx_qos -i $intf --pfc 1,0,0,0,0,0,0,0
    tc_wrap.py -i $intf
    tc_wrap.py -i $intf -u 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

    # To enable on other QoS, modify the above code accordingly
    # For eg., to enable PFC on QoS 1 or 2 us the code below

    # Qos 1
    #  mlnx_qos -i $intf --pfc 0,1,0,0,0,0,0,0
    #  tc_wrap.py -i $intf
    #  tc_wrap.py -i $intf -u 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1

    # Qos 2
    # mlnx_qos -i $intf --pfc 0,0,1,0,0,0,0,0
    # tc_wrap.py -i $intf
    # tc_wrap.py -i $intf -u 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
else
    echo "Disabling PFC..."
    sudo mlnx_qos -i $intf --pfc 0,0,0,0,0,0,0,0
fi


