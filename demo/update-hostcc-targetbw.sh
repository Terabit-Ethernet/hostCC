echo "Before"
cat /sys/module/hostcc_module/parameters/target_pcie_thresh
echo $1 > /sys/module/hostcc_module/parameters/target_pcie_thresh
echo "After"
cat /sys/module/hostcc_module/parameters/target_pcie_thresh