
test:
	env LD_LIBRARY_PATH=../../bin python3 simdgen.py
	env LD_LIBRARY_PATH=../../bin python3 simdgen.py "red_sum(log(cosh(x)))"

test_numpy:
	env LD_LIBRARY_PATH=../../bin python3 numpy-sample.py

bench:
	env LD_LIBRARY_PATH=../../bin python3 bench.py
