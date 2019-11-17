.PHONY: all build clean
ROOT_DIR:=$(strip $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST)))))/../..

build:
	cd build && emconfigure cmake ../ && make -j4

build-in-podman:
	podman run -rm --privileged -v "$(ROOT_DIR)":/work trzeci/emscripten /bin/sh -c 'make -C /work/subprojects/js/ build'

start-nginx:
	sudo podman run -d --privileged -v "$(ROOT_DIR)/subprojects/js/html":/usr/share/nginx/html:ro -p 8080:80 --name openjph-nginx nginx 

stop-nginx:
	sudo podman kill openjph-nginx || true
	sudo podman rm openjph-nginx || true
	
