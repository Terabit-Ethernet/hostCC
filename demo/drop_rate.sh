drops_before=$(sudo ethtool -S $2 | grep rx_discards_phy | awk '{ printf $2 }')
rx_before=$(sudo ethtool -S $2 | grep rx_packets_phy | awk '{ printf $2 }')
sleep $1
drops_after=$(sudo ethtool -S $2 | grep rx_discards_phy | awk '{ printf $2 }')
rx_after=$(sudo ethtool -S $2 | grep rx_packets_phy | awk '{ printf $2 }')
echo "print(($drops_after - $drops_before)/($rx_after - $rx_before) * 100)" | lua 
