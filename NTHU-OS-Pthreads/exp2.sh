READER_QUEUE_SIZE=200
WORKER_QUEUE_SIZE=200
WRITER_QUEUE_SIZE=4000
CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE=20
CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE=80
CONSUMER_CONTROLLER_CHECK_PERIOD=1000000

echo -e "========== Exp 2 Start =========="

touch ./result/exp2.out
> ./result/exp2.out

scl enable devtoolset-8 'make clean && make'

# Test low and high
# 1. Both larger chance to delete and create
# 2. Only increase delete/create and decreate the other one
CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGES=(\
# "1" \
# "20" \
# "49" \
"10" \
"30" \
"50" \
)
CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGES=(\
# "99" \
# "80" \
# "51" \
"50" \
"70" \
"90" \
)

for ((i=0; i<${#CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGES[@]}; i++)); do
    echo -e "T ${CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGES[i]} ${CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGES[i]}"
    ./exp 4000 ./tests/01.in ./tests/01.out ${READER_QUEUE_SIZE} ${WORKER_QUEUE_SIZE} ${WRITER_QUEUE_SIZE} \
    ${CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGES[i]} ${CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGES[i]} \
    ${CONSUMER_CONTROLLER_CHECK_PERIOD} >> ./result/exp2.out
    echo -e "T ${CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGES[i]} ${CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGES[i]}" >> ./result/exp2.out
done

# python3 ./scripts/generate_figure.py 2-1 y 0 1 2 
python3 ./scripts/generate_figure.py 2-2 y

echo -e "=========== Exp 2 Done ==========="