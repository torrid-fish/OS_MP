./scripts/auto_gen_transformer --input ./tests/00_spec.json
scl enable devtoolset-8 'make clean && make'
echo ===============================================
./main 200 ./tests/00.in ./tests/00.out
echo ===============================================

./scripts/auto_gen_transformer --input ./tests/01_spec.json
scl enable devtoolset-8 'make clean && make'
echo ===============================================
./main 4000 ./tests/01.in ./tests/01.out
echo ===============================================

# Verify
./scripts/verify --output ./tests/00.out --answer ./tests/00.ans
./scripts/verify --output ./tests/01.out --answer ./tests/01.ans