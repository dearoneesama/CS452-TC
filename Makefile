ifeq ($(JIXI_LOCAL), 1)
	XDIR:=~/Projects/cs452-project/arm-gnu-toolchain-12.2.rel1-darwin-x86_64-aarch64-none-elf
else ifeq ($(RUNNER_IMAGE), 1)
	XDIR:=/usr/cs452-public-xdev
else
	XDIR:=/u/cs452/public/xdev
endif
ARCH=cortex-a72
TRIPLE=aarch64-none-elf
XBINDIR:=$(XDIR)/bin
CXX:=$(XBINDIR)/$(TRIPLE)-g++
OBJCOPY:=$(XBINDIR)/$(TRIPLE)-objcopy
OBJDUMP:=$(XBINDIR)/$(TRIPLE)-objdump
OUTPUT := build

ETL_INCLUDE:=thirdparty/etl/include

# COMPILE OPTIONS
WARNINGS=-Wall -Wextra -Wpedantic -Wno-unused-const-variable
BENCHMARKING=0
OPTLVL=-O3
CFLAGS:= $(OPTLVL) -pipe -static $(WARNINGS) -ffreestanding -nostartfiles \
	-mcpu=$(ARCH) -static-pie -mstrict-align -fno-builtin -mgeneral-regs-only \
	-fno-rtti -fno-exceptions -nostdlib -lgcc -std=gnu++17 -I $(ETL_INCLUDE) -DBENCHMARKING=$(BENCHMARKING)

# -Wl,option tells g++ to pass 'option' to the linker with commas replaced by spaces
# doing this rather than calling the linker ourselves simplifies the compilation procedure
LDFLAGS:=-Wl,-nmagic -Wl,-Tlinker.ld

# Source files and include dirs
SOURCES := $(wildcard *.cpp) $(wildcard *.S)
# Create .o and .d files for every .cpp and .S (hand-written assembly) file
OBJECTS := $(patsubst %, $(OUTPUT)/%, $(patsubst %.cpp, %.o, $(patsubst %.S, %.o, $(SOURCES))))
DEPENDS := $(patsubst %, $(OUTPUT)/%, $(patsubst %.cpp, %.d, $(patsubst %.S, %.d, $(SOURCES))))

# The first rule is the default, ie. "make", "make all" and "make kernel8.img" mean the same
all: $(OUTPUT) $(OUTPUT)/kernel8.img

$(OUTPUT):
	mkdir -p $(OUTPUT)

clean:
	rm -rf $(OUTPUT)

$(OUTPUT)/kernel8.img: $(OUTPUT)/kernel8.elf
	$(OBJCOPY) $< -O binary $@

$(OUTPUT)/kernel8.elf: $(OBJECTS) linker.ld
	$(CXX) $(CFLAGS) $(filter-out %.ld, $^) -o $@ $(LDFLAGS)
	@$(OBJDUMP) -d $(OUTPUT)/kernel8.elf | fgrep -q q0 && printf "\n***** WARNING: SIMD INSTRUCTIONS DETECTED! *****\n\n" || true

$(OUTPUT)/%.o: %.cpp Makefile
	$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTPUT)/%.o: %.S Makefile
	$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPENDS)
