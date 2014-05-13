SHELL = /bin/bash

EXENAME = c3

CFLAGS = --std=c++11 --stdlib=libc++ -I. `llvm-config --cppflags`
LDFLAGS = -v -lc++ `llvm-config --ldflags --libs core support target`

SRCDIR = src
OBJDIR = obj

SRCS := $(shell find $(SRCDIR) -name '*.c') $(shell find $(SRCDIR) -name '*.cpp')
HDRS := $(shell find $(SRCDIR) -name '*.h') $(shell find $(SRCDIR) -name '*.hpp')

OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS)))

all: $(EXENAME)

$(OBJDIR): $(OBJDIR)/C3
	
$(OBJDIR)/C3:
	mkdir -p $(OBJDIR)/C3

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CXX) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

$(EXENAME): $(OBJDIR) $(OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

clean:
	if test -e $(EXENAME); then rm $(EXENAME); fi
	if test -d $(OBJDIR); then rm -r $(OBJDIR); fi
