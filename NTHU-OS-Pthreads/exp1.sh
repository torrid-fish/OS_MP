READER_QUEUE_SIZE=200
WORKER_QUEUE_SIZE=200
WRITER_QUEUE_SIZE=4000
CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE=20
CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE=80
CONSUMER_CONTROLLER_CHECK_PERIOD=1000000

echo -e "========== Exp 1 Start =========="

touch ./result/exp1.out
> ./result/exp1.out

scl enable devtoolset-8 'make clean && make'

# Test check period
CONSUMER_CONTROLLER_CHECK_PERIODS=(\
"100000" \
"1000000" \
"10000000" \
)

for ((i=0; i<${#CONSUMER_CONTROLLER_CHECK_PERIODS[@]}; i++)); do
    echo -e "T ${CONSUMER_CONTROLLER_CHECK_PERIODS[i]}"
    ./exp 4000 ./tests/01.in ./tests/01.out ${READER_QUEUE_SIZE} ${WORKER_QUEUE_SIZE} ${WRITER_QUEUE_SIZE} \
    ${CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE} ${CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE} \
    ${CONSUMER_CONTROLLER_CHECK_PERIODS[i]} >> ./result/exp1.out
    echo -e "T ${CONSUMER_CONTROLLER_CHECK_PERIODS[i]}" >> ./result/exp1.out
done

python3 ./scripts/generate_figure.py 1 y

echo -e "=========== Exp 1 Done ==========="