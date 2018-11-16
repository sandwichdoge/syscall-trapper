ARCH := $(shell uname -m | head -c 3)
FLAGS = -Wall

ifeq ($(ARCH),x86)
all: x86/syscall-trapper.c
@echo $(ARCH)
	gcc x86/syscall-trapper.c -o syscall-trapper
else
ifeq ($(ARCH),arm)
all: ARM/syscall-trapper.c
	@echo $(ARCH)
	gcc ARM/syscall-trapper.c -o syscall-trapper
endif
endif
