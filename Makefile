.PHONY: test simulator sanitize
test:
	$(MAKE) -C firmware/teensy test
	python3 -m unittest discover -s firmware/teensy/tests -p 'test_*.py'
simulator:
	$(MAKE) -C firmware/teensy all
sanitize:
	$(MAKE) -C firmware/teensy sanitize
