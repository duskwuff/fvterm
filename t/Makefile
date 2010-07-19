SUITES = dumb

test: $(foreach suite,$(SUITES),test-$(suite))

test-%:
	python runtest.py $*/*.t
