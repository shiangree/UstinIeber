sudo dmesg mp1 -c
make clean
make
sudo rmmod mp1.ko
sudo insmod mp1.ko
