# Copyright 2020 The Open SoC Debug Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LIBASAN_SO := $(shell find /usr/lib* -iname 'libasan.so.*' 2>/dev/null | sort -r | head -n1)
LSAN_SUPPRESSIONS := $(CURDIR)/lsan-python.supp
# -s is always passed to pytest to ensure that ASAN reports are actually
# visible. (Otherwise pytest fails with a non-zero exit code, but no reason for
# that is printed.)
PYTEST_ARGS := -vv

# Run pytest with a ASAN-enabled libosd build (configure option --with-asan)
pytest-asan:
	@if [ ! -f "$(LIBASAN_SO)" ]; then \
	  echo "libasan.so.* not found. Explicitly pass the path with"; \
		echo "make pytest-asan LIBASAN_SO=/path/to/libasan.so"; \
		exit 1; \
	fi
	@if [ ! -f "$(LSAN_SUPPRESSIONS)" ]; then \
	  echo "Suppression file $(LSAN_SUPPRESSIONS) not found."; \
		exit 1; \
	fi
	LSAN_OPTIONS=suppressions="$(LSAN_SUPPRESSIONS)" \
		LD_PRELOAD="$(LIBASAN_SO)" \
		PYTHONMALLOC=malloc \
		pytest -s $(PYTEST_ARGS)

devbuild:
	python3 setup.py build_ext --inplace --use-cython

clean:
	$(RM) -rf build
	$(RM) -f *.so
	$(RM) -f src/osd.c
