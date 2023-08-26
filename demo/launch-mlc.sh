echo "Launching mlc with $1 cores"
if [ "$1" = 1 ] 
then
    /home/saksham/mlc/Linux/mlc --loaded_latency -T -d0 -e -k1 -j0 -b1g -t1000 -W2
elif [ "$1" = 2 ] 
then
     /home/saksham/mlc/Linux/mlc --loaded_latency -T -d0 -e -k1,2 -j0 -b1g -t1000 -W2
elif [ "$1" = 3 ] 
then
    /home/saksham/mlc/Linux/mlc --loaded_latency -T -d0 -e -k1,2,3 -j0 -b1g -t1000 -W2
else
    /home/saksham/mlc/Linux/mlc --loaded_latency -T -d0 -e -k1,2,3 -j0 -b1g -t1000 -W2
fi