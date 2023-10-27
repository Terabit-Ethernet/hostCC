cd ../utils
sudo bash set_mba_levels.sh
cd -
mkdir -p logs
sudo modprobe msr
sudo pkill -9 -f pcm
sudo pkill -9 -f pcm
sudo taskset -c 27 python3 app.py &
sleep 3
sudo taskset -c 23 python3 collect_pcie_bw.py > col_pcie.log &
sudo taskset -c 26 python3 collect_mem_bw.py > col_membw.log &
sudo taskset -c 22 python3 collect_iio_occ.py > col_iio.log &
sudo taskset -c 25 python3 collect_drop_rate.py > col_drops.log &
echo '128' > req-value.txt
sudo taskset -c 21 python3 collect_latency.py > col_lat.log &