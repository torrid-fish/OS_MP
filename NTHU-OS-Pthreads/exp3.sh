READER_QUEUE_SIZE=200
WORKER_QUEUE_SIZE=200
WRITER_QUEUE_SIZE=4000
CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE=20
CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE=80
CONSUMER_CONTROLLER_CHECK_PERIOD=1000000

echo -e "========== Exp 3 Start =========="

touch ./result/exp3.out
> ./result/exp3.out

scl enable devtoolset-8 'make clean && make'

# Test different sizes
WORKER_QUEUE_SIZES=(\
"10" \
"200" \
"4000" \
)

for ((i=0; i<${#WORKER_QUEUE_SIZES[@]}; i++)); do
    echo -e "T ${WORKER_QUEUE_SIZES[i]}"
    ./exp 4000 ./tests/01.in ./tests/01.out ${READER_QUEUE_SIZE} ${WORKER_QUEUE_SIZES[i]} ${WRITER_QUEUE_SIZE} \
    ${CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE} ${CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE} \
    ${CONSUMER_CONTROLLER_CHECK_PERIOD} >> ./result/exp3.out
    echo -e "T ${WORKER_QUEUE_SIZES[i]}" >> ./result/exp3.out
done

python3 ./scripts/generate_figure.py 3 y

echo -e "=========== Exp 3 Done ==========="