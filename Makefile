# Makefile
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

SANITIZERS = none debug msan asan usan tsan
.PHONY: default gcc clang run update check ce todo distclean clean build test all $(SANITIZERS)

COMPILER=system
CXX_BASE=$(CXX:$(dir $(CXX))%=%)
ifeq ($(CXX_BASE),g++)
    COMPILER=gcc
endif
ifeq ($(CXX_BASE),clang++)
    COMPILER=clang
endif

CXX_FLAGS = -g
SANITIZER = none
SOURCEDIR = $(shell pwd)
BUILDROOT = build
BUILD     = $(BUILDROOT)/$(COMPILER)/$(SANITIZER)
EXAMPLE   = server
CMAKE_C_COMPILER=$(COMPILER)
CMAKE_CXX_COMPILER=$(COMPILER)

ifeq ($(SANITIZER),none)
    CXX_FLAGS = -O3 -pedantic -Wall -Wextra -Werror
endif
ifeq ($(SANITIZER),debug)
    CXX_FLAGS = -g
endif
ifeq ($(SANITIZER),usan)
    SAN_FLAGS = -fsanitize=memory
    BUILD     = $(BUILDROOT)/msan
endif
ifeq ($(SANITIZER),asan)
    SAN_FLAGS = -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize-address-use-after-scope

    BUILD     = $(BUILDROOT)/asan
endif
ifeq ($(SANITIZER),usan)
    SAN_FLAGS = -fsanitize=undefined
    BUILD     = $(BUILDROOT)/usan
endif
ifeq ($(SANITIZER),tsan)
    SAN_FLAGS = -fsanitize=thread
    BUILD     = $(BUILDROOT)/tsan
endif
ifeq ($(SANITIZER),lsan)
    SAN_FLAGS = -fsanitize=leak
    BUILD     = $(BUILDROOT)/lsan
endif

default: test

all: $(SANITIZERS)

gcc:
	$(MAKE) CXX=/opt/gcc-14.1.0/bin/g++

clang:
	$(MAKE) CXX=/opt/llvm-18.1.8/bin/clang++

run: build
	./$(BUILD)/examples/$(EXAMPLE)

none: test

$(SANITIZERS):
	$(MAKE) SANITIZER=$@

build:
	@mkdir -p $(BUILD)
	cd $(BUILD); CC=$(CXX) cmake $(SOURCEDIR) $(TOOLCHAIN) $(SYSROOT) -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_CXX_FLAGS="$(CXX_FLAGS) $(SAN_FLAGS)"
	cmake --build $(BUILD)

test: build
	cd $(BUILD); $(MAKE) test

ce:
	@mkdir -p $(BUILD)
	bin/mk-compiler-explorer.py $(BUILD)

SOURCE_CMAKELISTS = src/beman/net29/CMakeLists.txt
update:
	bin/update-cmake-headers.py $(SOURCE_CMAKELISTS)

check:
	@for h in `find include -name \*.hpp`; \
	do \
		from=`echo -n $$h | sed -n 's@.*Beman/\(.*\).hpp.*@\1@p'`; \
		< $$h sed -n "/^ *# *include <Beman\//s@.*[</]Beman/\(.*\).hpp>.*@$$from \1@p"; \
	done | tsort > /dev/null

todo:
	bin/mk-todo.py

clean:
	$(RM) mkerr olderr *~

distclean: clean
	$(RM) -r $(BUILDROOT)
