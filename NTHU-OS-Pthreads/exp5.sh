READER_QUEUE_SIZE=200
WORKER_QUEUE_SIZE=200
WRITER_QUEUE_SIZE=4000
CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE=20
CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE=80
CONSUMER_CONTROLLER_CHECK_PERIOD=1000000

echo -e "========== Exp 5 Start =========="

touch ./result/exp5.out
> ./result/exp5.out

scl enable devtoolset-8 'make clean && make'

# Smaller reader queue size
READER_QUEUE_SIZES=(\
"1" \
"200" \
)
WRITER_QUEUE_SIZES=(\
"4000" \
"4000" \
)

for ((i=0; i<${#READER_QUEUE_SIZES[@]}; i++)); do
    echo -e "T ${READER_QUEUE_SIZES[i]} ${WRITER_QUEUE_SIZES[i]}"
    ./exp 4000 ./tests/01.in ./tests/01.out ${READER_QUEUE_SIZES[i]} ${WORKER_QUEUE_SIZE} ${WRITER_QUEUE_SIZES[i]} \
    ${CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE} ${CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE} \
    ${CONSUMER_CONTROLLER_CHECK_PERIOD} >> ./result/exp5.out
    echo -e "T ${READER_QUEUE_SIZES[i]} ${WRITER_QUEUE_SIZES[i]}" >> ./result/exp5.out
done

python3 ./scripts/generate_figure.py 5 y

echo -e "=========== Exp 5 Done ==========="