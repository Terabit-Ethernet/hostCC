
# default experiment
sudo bash run-dctcp-tput-experiment.sh -E "worst_case" -M 9978 --num_servers 40 --num_clients 40 -c "0,4,8,12,16,20,24,28" --ring_buffer 8192 --buf 1 --mlc_cores 'none' --bandwidth "100g" --server_intf ens2f1np1


