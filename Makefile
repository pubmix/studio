.PHONY: test simulator sanitize
test:
	$(MAKE) -C firmware/teensy test
	python3 -m unittest discover -s firmware/teensy/tests -p 'test_*.py'
simulator:
	$(MAKE) -C firmware/teensy all
sanitize:
	$(MAKE) -C firmware/teensy sanitize

# Optional ML-free service checks; install services/stem-engine .[test] first.
STEM_PYTHON ?= services/stem-engine/.venv/bin/python
.PHONY: test-stems
test-stems:
	PYTHONPATH=services/stem-engine $(STEM_PYTHON) -m pytest services/stem-engine/tests -q
