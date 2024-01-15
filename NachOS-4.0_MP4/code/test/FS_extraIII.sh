../build.linux/nachos -f
../build.linux/nachos -mkdir /T0
../build.linux/nachos -mkdir /T1
../build.linux/nachos -mkdir /T0/0
../build.linux/nachos -mkdir /T0/1
../build.linux/nachos -mkdir /T0/2
../build.linux/nachos -cp ./num_100.txt /T0/0/F1
../build.linux/nachos -cp ./num_100.txt /T0/1/F1
../build.linux/nachos -cp ./num_100.txt /T0/2/F1
../build.linux/nachos -cp ./num_100.txt /T0/F1
../build.linux/nachos -lr /
echo ===================
# Delete a file
../build.linux/nachos -rr /T0/F1 
../build.linux/nachos -lr /
echo ===================
# Delete a directory
../build.linux/nachos -rr /T0/0 
../build.linux/nachos -lr /
echo ===================
# Delete a directory
../build.linux/nachos -rr /T0 
../build.linux/nachos -lr /



