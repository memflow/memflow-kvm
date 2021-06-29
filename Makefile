obj-y += memflow-kmod/

MCFLAGS += -O3
ccflags-y += ${MCFLAGS}
CC += ${MCFLAGS}

ifndef KERNELDIR
	KDIR := /lib/modules/$(shell uname -r)/build
else
	KDIR := $(KERNELDIR)
endif

ifndef OUT_DIR
	KOUTPUT := $(PWD)/build
else
	KOUTPUT := $(OUT_DIR)
endif

KOUTPUT_MAKEFILE := $(KOUTPUT)/Makefile

all: $(KOUTPUT_MAKEFILE)
	@echo "$(KOUTPUT)"

	make -C $(KDIR) M=$(KOUTPUT) src=$(PWD)/memflow-kmod modules

$(KOUTPUT):
	mkdir -p "$@"

$(KOUTPUT_MAKEFILE): $(KOUTPUT)
	touch "$@"

clean:
	make -C $(KDIR) M=$(KOUTPUT) src=$(PWD)/memflow-kmod clean
	$(shell rm $(KOUTPUT_MAKEFILE))
	rmdir $(KOUTPUT)
