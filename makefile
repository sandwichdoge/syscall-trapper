arch := $(shell uname -m)

ifeq ($(arch),x86_64)
all: x86/syscall-trapper.c
	@echo $(arch)
	gcc x86/syscall-trapper.c -o syscall-trapper
endif