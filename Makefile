.PHONY: all build clean
ROOT_DIR:=$(strip $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST)))))

all: build-in-podman build-js-in-podman

build-in-podman:
	podman run -rm --privileged -v "$(ROOT_DIR)":/work rikorose/gcc-cmake /bin/sh -c 'make -C /work build'

build-js-in-podman:
	make -C subprojects/js/ build-in-podman

build:
	cd build && cmake ../ && make -j4

build-js-in-podman:
	make -C subprojects/js/ build-in-podman

clean:
	git clean -dnfx
	@echo -n "Are you sure? [y/N] " && read ans && [ $${ans:-N} = y ] && git clean -dfx

