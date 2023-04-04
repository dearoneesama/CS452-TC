ifeq ($(JIXI_LOCAL), 1)
	XDIR:=./cs452-public-xdev
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
FPM_INCLUDE:=thirdparty/fpm/include

# dbg
ifeq ($(IS_TRACK_A), 1)
	IS_TRACK_A_CFLAG+=-DIS_TRACK_A=1
else
	IS_TRACK_A_CFLAG+=-DIS_TRACK_A=0
endif

ifeq ($(NO_CTS), 1)
	NO_CTS_CFLAG+=-DNO_CTS=1
else
	NO_CTS_CFLAG+=-DNO_CTS=0
endif

ifeq ($(DEBUG_PI), 1)
	DEBUG_PI_CFLAG+=-DDEBUG_PI=1
else
	DEBUG_PI_CFLAG+=-DDEBUG_PI=0
endif

# COMPILE OPTIONS
WARNINGS=-Wall -Wextra -Wpedantic -Wno-unused-const-variable
BENCHMARKING=0
OPTLVL=-O3
CFLAGS:= $(OPTLVL) -pipe -static $(WARNINGS) -ffreestanding -nostartfiles \
	-mcpu=$(ARCH) -static-pie -mstrict-align -fno-builtin -mgeneral-regs-only \
	-fno-rtti -fno-exceptions -nostdlib -lgcc -fno-use-cxa-atexit -fno-threadsafe-statics -std=gnu++17 \
	-isystem $(ETL_INCLUDE) -isystem $(FPM_INCLUDE) -DBENCHMARKING=$(BENCHMARKING) \
	$(IS_TRACK_A_CFLAG) $(NO_CTS_CFLAG) $(DEBUG_PI_CFLAG)

# -Wl,option tells g++ to pass 'option' to the linker with commas replaced by spaces
# doing this rather than calling the linker ourselves simplifies the compilation procedure
LDFLAGS:=-Wl,-nmagic -Wl,-Tlinker.ld

# Source files and include dirs
SOURCES := $(wildcard *.cpp) $(wildcard kern/*.cpp) $(wildcard generic/*.cpp) $(wildcard kern/*.S)
# Create .o and .d files for every .cpp and .S (hand-written assembly) file
OBJECTS := $(patsubst %, $(OUTPUT)/%, $(patsubst %.cpp, %.o, $(patsubst %.S, %.o, $(notdir $(SOURCES)))))
DEPENDS := $(patsubst %, $(OUTPUT)/%, $(patsubst %.cpp, %.d, $(patsubst %.S, %.d, $(notdir $(SOURCES)))))

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

$(OUTPUT)/%.o: %.cpp Makefile doit.sh
	$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTPUT)/%.o: kern/%.cpp Makefile doit.sh
	$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTPUT)/%.o: kern/%.S Makefile doit.sh
	$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTPUT)/%.o: generic/%.cpp Makefile doit.sh
	$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPENDS)
