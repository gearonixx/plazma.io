.PHONY: all build-client build-server run-client run-server clean format

all: build-client build-server

build-client:
	$(MAKE) -C Plazma build

run-client:
	$(MAKE) -C Plazma run

build-server:
	$(MAKE) -C PlazmaServer build


run-server:
	$(MAKE) -C PlazmaServer run


clean:
	$(MAKE) -C Plazma clean
	$(MAKE) -C PlazmaServer dist-clean

format:
	$(MAKE) -C Plazma format
	$(MAKE) -C PlazmaServer format
