# NIC Rx Ring buffer size
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-2048_8core" -M 4000 --num_servers 8 --num_clients 8 -c "0,4,8,12,16,20,24,28" --ring_buffer 2048 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-1024_8core" -M 4000 --num_servers 8 --num_clients 8 -c "0,4,8,12,16,20,24,28" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-512_8core" -M 4000 --num_servers 8 --num_clients 8 -c "0,4,8,12,16,20,24,28" --ring_buffer 512 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-256_8core" -M 4000 --num_servers 8 --num_clients 8 -c "0,4,8,12,16,20,24,28" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-2048_7core" -M 4000 --num_servers 7 --num_clients 7 -c "4,8,12,16,20,24,28" --ring_buffer 2048 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-1024_7core" -M 4000 --num_servers 7 --num_clients 7 -c "4,8,12,16,20,24,28" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-512_7core" -M 4000 --num_servers 7 --num_clients 7 -c "4,8,12,16,20,24,28" --ring_buffer 512 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1
sudo bash run-dctcp-tput-experiment.sh -E "ring_buffer-256_7core" -M 4000 --num_servers 7 --num_clients 7 -c "4,8,12,16,20,24,28" --ring_buffer 256 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

