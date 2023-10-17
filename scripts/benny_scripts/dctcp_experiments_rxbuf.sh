
# default experiment
sudo bash run-dctcp-tput-experiment.sh -E "baseline" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

# tcp Recieve buffer experiments
sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-.5" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 1024 --buf .5 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-.25" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 1024 --buf .25 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-1" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 1024 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-2" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 1024 --buf 2 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

sudo bash run-dctcp-tput-experiment.sh -E "tcp-buf-4" -M 4000 --num_servers 5 --num_clients 5 -c "4,8,12,16,20" --ring_buffer 1024 --buf 4 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1

