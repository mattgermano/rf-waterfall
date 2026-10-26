build_dir := build
build_type := Release

all: build

$(build_dir)/build.ninja: conanfile.py
	$(MAKE) reconfigure

.PHONY: configure
configure: $(build_dir)/build.ninja

.PHONY: reconfigure
reconfigure:
	rm -r $(build_dir) &> /dev/null || true && \
	conan install . --output-folder=$(build_dir) --build=missing -s '&:build_type=$(build_type)' -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True -s compiler.cppstd=23 && \
	. $(build_dir)/conanbuild.sh && \
	meson setup --native-file $(build_dir)/conan_meson_native.ini $(build_dir) . && \
	ninja -C $(build_dir) -t compdb > compile_commands.json

.PHONY: build
build: configure
	. $(build_dir)/conanbuild.sh && \
	ninja -C $(build_dir)

.PHONY: format
format:
	find src/ -type f -exec clang-format -i {} \;

.PHONY: tidy
tidy:
	run-clang-tidy -fix -format $(shell find src/ -type f)
