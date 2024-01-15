#!/bin/sh

# Create output file under code
touch output.txt

#DBG="-d z"
DBG=""

TC_NORMAL=(\
"-ep hw3t1 0 -ep hw3t2 0 $DBG" \
"-ep hw3t1 50 -ep hw3t2 50 $DBG" \
"-ep hw3t1 50 -ep hw3t2 90 $DBG" \
"-ep hw3t1 100 -ep hw3t2 100 $DBG" \
"-ep hw3t1 40 -ep hw3t2 55 $DBG" \
"-ep hw3t1 40 -ep hw3t2 90 $DBG" \
"-ep hw3t1 90 -ep hw3t2 100 $DBG" \
"-ep hw3t1 60 -ep hw3t3 50 $DBG" \
"-ep hw3t1 0 -ep hw3t3 50 -ep hw3t2 100 -ep hw3t1 50 $DBG" \
"-ep hw3t1 0 -ep hw3t1 50 -ep hw3t2 100 -ep hw3t2 50 -ep hw3t1 100 $DBG" \
"-ep hw3t1 60 -ep hw3t3 0 -ep hw3t1 40 -ep hw3t3 100 $DBG" \
"-ep hw3t2 60 -ep hw3t1 10 -ep hw3t1 40 -ep hw3t3 0 $DBG" \
)

TIMEOUT="timeout 0.75s"

# Check if script is in the right directory
if [ ${PWD##*/} != "code" ]; then 
    echo -e "\n@@@@@ Script should be put in ./code @@@@@\n"
    exit 1
fi

# Build nachos
echo -e "===== Make nachos ====="
cd build.linux/
make clean > /dev/null 2>&1
make -j2 > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "Nachos built failed"
    exit 1
else
    echo -e "Nachos built successfully"
fi
cd ..

# Build test cases
echo -e "===== Make test cases ====="
cd test
make clean > /dev/null 2>&1
make hw3t1 hw3t2 hw3t3 > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "Tests built failed"
    exit 1
else
    echo -e "Tests built successfully"
fi

# Clean the content of output
> ../output.txt

# Start testing
echo -e "===== Start testcases ====="
for ((i=0; i<${#TC_NORMAL[@]}; i++)); do
    echo -e "===== Test case $(($i+1)): ${TC_NORMAL[$i]} =====" >> ../output.txt 2>&1
    $TIMEOUT ../build.linux/nachos ${TC_NORMAL[$i]} >> ../output.txt 2>&1
done

echo -e "Run testcases sucessfully"

# Start testing
cd ..
echo -e "===== Start compare ====="
diff output.txt sample_output.txt

exit 0
