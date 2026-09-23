.PHONY: test simulator sanitize
test:
	$(MAKE) -C firmware/teensy test
	python3 -m unittest discover -s firmware/teensy/tests -p 'test_*.py'
simulator:
	$(MAKE) -C firmware/teensy all
sanitize:
	$(MAKE) -C firmware/teensy sanitize

PYTHON ?= python3
.PHONY: integration-test
integration-test: simulator
	STUDIO_FIRMWARE_VALIDATOR=$(CURDIR)/firmware/teensy/tools/validate_prepared.py $(PYTHON) -B -m unittest discover -s services/preparation-adapter -p 'test_*.py'
	PYTHONPATH=$(CURDIR)/services/stem-engine:$(CURDIR)/services/preparation-adapter $(PYTHON) -B -m unittest discover -s tests/integration -p 'test_*.py'
