.PHONY: all mods dist clean

all: dist

pasm/pasm:
	cd pasm/pasm_source && sh linuxbuild

mods: pasm/pasm
	make -C mods all

dist: mods
	mkdir -p dist/root/punchpress/mods
	cp -f mods/modules.order mods/Module.symvers mods/pruss/pruss.ko mods/sim/spi_slave.ko mods/sim/sim.ko dist/root/punchpress/mods

	mkdir -p dist/root/punchpress
	cp -rf visserver dist/root/punchpress

	cp -rf bbb/* dist/root/punchpress

clean:
	make -C mods clean
