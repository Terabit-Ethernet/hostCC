
# default experiment
sudo bash run-dctcp-tput-experiment.sh -E "baseline" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

# tcp Recieve buffer experiments
sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-.5" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf .5 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-.25" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf .25 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-2" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 2 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-4" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 4 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

# tcp MTU size experiments
sudo bash run-dctcp-tput-experiment.sh -E "MTU8k" -M 8000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "MTU9k" -M 9000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "MTU2k" -M 2000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "MTU1k" -M 1000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "MTU.5k" -M 500 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

# tcp bandwidth experiments
sudo bash run-dctcp-tput-experiment.sh -E "bandwidth-5" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "5g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "bandwidth-10" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "10g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "bandwidth-15" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "15g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "bandwidth-20" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "20g" --server_intf ens2f1np1

# host memory bandwidth contention 
sudo bash run-dctcp-tput-experiment.sh -E "memory_contention-1" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores '1' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "memory_contention-2" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores '1,2' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "memory_contention-3" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 1024 --buf 1 --mlc_cores '1,2,3' --bandwidth "100g" --server_intf ens2f1np1

# NIC Rx Ring buffer size
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-512" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 512 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-256" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-2048" -M 4000 --num_servers 4 --num_clients 4 -c "4,8,12,16" --ring_buffer 2048 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1