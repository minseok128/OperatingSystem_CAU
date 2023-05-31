cd /root/os161/src/kern/test
git pull origin main
git log
cd /root/os161/src/kern/conf
./config DUMBVM

cd ../compile/DUMBVM
bmake depend
bmake

bmake install

cd /root/os161/root
sys161 kernel