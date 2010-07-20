SUITES = \
	 dumb \
	 vt100

test: $(foreach suite,$(SUITES),test-$(suite))

test-%:
	python runtest.py $*/*.t
