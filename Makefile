GCC_VER=$(shell $(PRE)$(CC) -dumpversion)
UNAME_S=$(shell uname -s)
ARCH?=$(shell uname -m)
ifeq ($(UNAME_S),Linux)
  OS=Linux
endif
ifeq ($(findstring MINGW64,$(UNAME_S)),MINGW64)
  OS=mingw64
  CFLAGS+=-D__USE_MINGW_ANSI_STDIO=1
endif
ifeq ($(findstring CYGWIN,$(UNAME_S)),CYGWIN)
  OS=cygwin
endif
ifeq ($(UNAME_S),Darwin)
  ifeq ($(ARCH),x86_64)
    OS=mac
  else
    OS=mac-m1
  endif
  LIB_SUF=dylib
else
  LIB_SUF=so
endif
ifeq ($(UNAME_S),OpenBSD)
  OS=openbsd
  CXX?=clang++
  CFLAGS+=-I/usr/local/include
  LDFLAGS+=-L/usr/local/lib
endif
ifeq ($(UNAME_S),FreeBSD)
  OS=freebsd
  CXX?=clang++
  CFLAGS+=-I/usr/local/include
  LDFLAGS+=-L/usr/local/lib
endif

ifneq ($(findstring $(ARCH),x86_64/amd64),)
  CPU=x86-64
  INTEL=1
  BIT=64
  BIT_OPT=-m64
  #ASM=nasm -felf64
  XBYAK?=1
endif
ifeq ($(ARCH),x86)
  CPU=x86
  INTEL=1
  BIT=32
  BIT_OPT=-m32
endif
ifneq ($(findstring $(ARCH),armv7l/armv6l),)
  CPU=arm
  BIT=32
endif
ifneq ($(findstring $(ARCH),aarch64/arm64),)
  CPU=aarch64
  BIT=64
  XBYAK_AARCH64?=1
endif
ifeq ($(findstring $(OS),mac/mac-m1/mingw64/openbsd),)
  LDFLAGS+=-lrt
endif
ifeq ($(ARCH),s390x)
  CPU=systemz
  BIT=64
endif

CP=cp -f
AR=ar r
MKDIR=mkdir -p
RM=rm -rf

ifeq ($(DEBUG),1)
  ifeq ($(ASAN),1)
    CFLAGS+=-fsanitize=address
    LDFLAGS+=-fsanitize=address
  endif
else
  CFLAGS_OPT+=-fomit-frame-pointer -DNDEBUG -fno-stack-protector
  CFLAGS_OPT+=-O3
endif
CFLAGS_WARN=-Wall -Wextra -Wformat=2 -Wcast-qual -Wcast-align -Wwrite-strings -Wfloat-equal -Wpointer-arith -Wundef
CFLAGS+=-g3
INC_OPT=-I include -I test -I src
CFLAGS+=$(CFLAGS_WARN) $(BIT_OPT) $(INC_OPT)
DEBUG=0
CFLAGS_OPT_USER?=$(CFLAGS_OPT)
ifeq ($(DEBUG),0)
CFLAGS+=$(CFLAGS_OPT_USER)
endif
CFLAGS+=$(CFLAGS_USER)
LDFLAGS+=$(BIT_OPT) $(LDFLAGS_USER)
CFLAGS+=-fPIC

LIB_DIR=lib
OBJ_DIR=obj
EXE_DIR=bin
SRC_SRC=main.cpp
TEST_SRC=parser_test.cpp
LIB_OBJ=$(OBJ_DIR)/main.o
ifeq ($(CPU),x86-64)
  MCL_USE_XBYAK?=1
  CFLAGS+=-I src/x64
endif

ifeq ($(XBYAK),1)
  CFLAGS+=-I ext/xbyak
endif
ifeq ($(XBYAK_AARCH64),1)
  CFLAGS+=-I ext/xbyak_aarch64
  LDFLAGS+=-L ext/xbyak_aarch64/lib -lxbyak_aarch64
endif

##################################################################
SG_LIB=$(LIB_DIR)/libsimdgen.a
all: $(SG_LIB)

$(SG_LIB): $(LIB_OBJ)
	$(AR) $@ $(LIB_OBJ)

##################################################################

VPATH=test src

.SUFFIXES: .cpp .d .exe .c .o

$(OBJ_DIR)/%.o: %.cpp
	$(PRE)$(CXX) $(CFLAGS) -c $< -o $@ -MMD -MP -MF $(@:.o=.d)

$(EXE_DIR)/%.exe: $(OBJ_DIR)/%.o $(SG_LIB)
	$(PRE)$(CXX) $< -o $@ $() $(LDFLAGS)

SAMPLE_EXE=$(addprefix $(EXE_DIR)/,$(addsuffix .exe,$(basename $(SAMPLE_SRC))))
sample: $(SAMPLE_EXE) $()

TEST_EXE=$(addprefix $(EXE_DIR)/,$(TEST_SRC:.cpp=.exe))
test_ci: $(TEST_EXE)
	@sh -ec 'for i in $(TEST_EXE); do echo $$i; env LSAN_OPTIONS=verbosity=0:log_threads=1 $$i; done'
test: $(TEST_EXE)
	@echo test $(TEST_EXE)
	@sh -ec 'for i in $(TEST_EXE); do $$i|grep "ctest:name"; done' > result.txt
	@grep -v "ng=0, exception=0" result.txt; if [ $$? -eq 1 ]; then echo "all unit tests succeed"; else exit 1; fi

clean:
	$(RM) $(LIB_DIR)/*.a $(LIB_DIR)/*.$(LIB_SUF) $(OBJ_DIR)/*.o $(OBJ_DIR)/*.obj $(OBJ_DIR)/*.d $(EXE_DIR)/*.exe

ALL_SRC=$(SRC_SRC) $(TEST_SRC) $(SAMPLE_SRC)
DEPEND_FILE=$(addprefix $(OBJ_DIR)/, $(addsuffix .d,$(basename $(ALL_SRC))))
-include $(DEPEND_FILE)

PREFIX?=/usr/local
install: lib/libsimdgen.a
	$(MKDIR) $(PREFIX)/include/simdgen
	cp -a include/simdgen $(PREFIX)/include/
	$(MKDIR) $(PREFIX)/lib
	cp -a lib/libsimdgen.a  $(PREFIX)/lib/

.PHONY: test

# don't remove these files automatically
.SECONDARY: $(addprefix $(OBJ_DIR)/, $(ALL_SRC:.cpp=.o))

