
# default experiment
sudo bash run-dctcp-tput-experiment.sh -E "baseline" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

# tcp Recieve buffer experiments
sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-.5" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf .5 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-.25" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf .25 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-1" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-2" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 2 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-4" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 4 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

# tcp MTU size experiments
sudo bash run-dctcp-tput-experiment.sh -E "MTU9k" -M 9000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "MTU8k" -M 8000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "MTU4k" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "MTU2k" -M 2000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "MTU1k" -M 1000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "MTU.5k" -M 500 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

# tcp bandwidth experiments
sudo bash run-dctcp-tput-experiment.sh -E "bandwidth-5" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "5g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "bandwidth-10" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "10g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "bandwidth-15" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "15g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "bandwidth-20" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "20g" --server_intf ens2f1np1

# host memory bandwidth contention 
sudo bash run-dctcp-tput-experiment.sh -E "memory_contention-1" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores '1' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "memory_contention-2" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores '1,2' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "memory_contention-3" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores '1,2,3' --bandwidth "100g" --server_intf ens2f1np1

# NIC Rx Ring buffer size
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-2048" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 2048 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-256" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-512" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 512 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-256" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

# number of flows 
sudo bash run-dctcp-tput-experiment.sh -E "flow1" -M 4000 --num_servers 1 --num_clients 1 -c "4" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "flow2" -M 4000 --num_servers 2 --num_clients 2 -c "4,8" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "flow3" -M 4000 --num_servers 3 --num_clients 3 -c "4,8,12" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "flow4" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "flow5" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "flow6" -M 4000 --num_servers 6 --num_clients 6 -c "4,8,12,16,20,24" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "flow7" -M 4000 --num_servers 7 --num_clients 7 -c "4,8,12,16,20,24,28" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "flow8" -M 4000 --num_servers 8 --num_clients 8 -c "4,8,12,16,20,24,28,32" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "flow20" -M 4000 --num_servers 20 --num_clients 20 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "flow40" -M 4000 --num_servers 40 --num_clients 40 -c "4,8,12,16,20" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1


# might also be interesting to do more flows, but keep the number of cores the same 
