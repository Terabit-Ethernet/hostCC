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
