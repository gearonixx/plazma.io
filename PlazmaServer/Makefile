CMAKE_COMMON_FLAGS ?= -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

CMAKE_DEBUG_FLAGS ?= -DUSERVER_SANITIZE='addr ub'
CMAKE_RELEASE_FLAGS ?=

CMAKE_RELEASE_FLAGS += -DCMAKE_BUILD_TYPE=Release $(CMAKE_COMMON_FLAGS)

#CMAKE_OS_FLAGS ?= -DUSERVER_FEATURE_CRYPTOPP_BLAKE2=0 -DUSERVER_FEATURE_REDIS_HI_MALLOC=1
CLANG_FORMAT ?= clang-format
DOCKER_COMPOSE ?= docker-compose

ifeq ($(KERNEL),Darwin)
CMAKE_COMMON_FLAGS += -DUSERVER_NO_WERROR=1 -DUSERVER_CHECK_PACKAGE_VERSIONS=0 \
  -DUSERVER_DOWNLOAD_PACKAGE_CRYPTOPP=1 \
  -DOPENSSL_ROOT_DIR=$(shell brew --prefix openssl) \
  -DUSERVER_PG_INCLUDE_DIR=$(shell pg_config --includedir) \
  -DUSERVER_PG_LIBRARY_DIR=$(shell pg_config --libdir) \
  -DUSERVER_PG_SERVER_LIBRARY_DIR=$(shell pg_config --pkglibdir) \
  -DUSERVER_PG_SERVER_INCLUDE_DIR=$(shell pg_config --includedir-server)
endif

#CMAKE_DEBUG_FLAGS += -DCMAKE_BUILD_TYPE=Debug $(CMAKE_COMMON_FLAGS)

#.PHONY: all
#all: test-debug test-release
#
## Run cmake
#.PHONY: cmake-debug
#cmake-debug:
#	cmake -B build_debug $(CMAKE_DEBUG_FLAGS)
#
#.PHONY: cmake-release
#cmake-release:
#	cmake -B build_release $(CMAKE_RELEASE_FLAGS)
#
#build_debug/CMakeCache.txt:
#	$(MAKE) cmake-debug
#
#build_release/CMakeCache.txt:
#	$(MAKE) cmake-release
#
## Build using cmake
#.PHONY: build-debug build-release
#build-debug build-release: build-%: build_%/CMakeCache.txt
#	cmake --build build_$* -j $(NPROCS) --target realmedium_sample
#
## Test
#.PHONY: test-debug test-release
#test-debug test-release: test-%: build-%
#	cmake --build build_$* -j $(NPROCS) --target realmedium_sample_unittest
#	cd build_$* && ((test -t 1 && GTEST_COLOR=1 PYTEST_ADDOPTS="--color=yes" ctest -V) || ctest -V)
#	pycodestyle tests

# Start the service (via testsuite service runner)
# Cleanup data

# Install
# Format the sources
.PHONY: format
format:
	find src -name '*pp' -type f | xargs $(CLANG_FORMAT) -i
#
## Set environment for --in-docker-start
#export DB_CONNECTION := postgresql://${POSTGRES_USER}:${POSTGRES_PASSWORD}@service-postgres:5432/${POSTGRES_DB}
#
## Internal hidden targets that are used only in docker environment
## defines the matches  (release, debug)
#--in-docker-start-debug --in-docker-start-release: --in-docker-start-%: install-%
#	mkdir -p ./postgresql/data
#	touch ./postgresql/data/initial_data.sql
#	# init the initial_data sql file probably
#	psql ${DB_CONNECTION} -f ./postgresql/data/initial_data.sql
#	/home/user/.local/bin/realmedium_sample \
#		--config /home/user/.local/etc/realmedium_sample/config.yaml \
#		--config_var s /home/user/.local/etc/realmedium_sample/config_vars.docker.yaml

.PHONY: --in-start
--in-start:
	cmake -B build $(CMAKE_RELEASE_FLAGS)
	cmake --build build -j$$(nproc) --target realmedium_sample
	cmake --install build -v --component realmedium_sample

	/home/user/.local/bin/realmedium_sample \
		--config /home/user/.local/etc/realmedium_sample/config.yaml \
		--config_vars /home/user/.local/etc/realmedium_sample/config_vars.docker.yaml


.PHONY: build
build:
	$(DOCKER_COMPOSE) build --no-cache

.PHONY: run
run:
	$(DOCKER_COMPOSE) up -d

.PHONY: start
start:
	$(DOCKER_COMPOSE) exec plazma-server make -- --in-start

.PHONY: restart
restart:
	$(DOCKER_COMPOSE) exec plazma-server pkill -f realmedium_sample || true
	$(DOCKER_COMPOSE) exec plazma-server make -- --in-start

#.PHONY: clean
#clean:
#	cmake --build build --target clean

.PHONY: dist-clean
dist-clean:
	rm -rf build
	rm -rf tests
	rm -rf tests/.pytest_cache

.PHONY: down
down:
	$(DOCKER_COMPOSE) down -v

