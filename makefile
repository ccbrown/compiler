SHELL = /bin/bash

EXENAME = c3

CXXFLAGS = --std=c++11 --stdlib=libc++ `llvm-config --cppflags`
LDFLAGS = -v -lc++ `llvm-config --ldflags --libs core support target`

SRCDIR = src
OBJDIR = obj

SRCS := $(shell find $(SRCDIR) -name '*.cpp')
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.d,$(SRCS))

all: $(EXENAME)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), $(NODEPS))))
-include $(DEPS)
endif

$(OBJDIR): $(OBJDIR)/C3
	
$(OBJDIR)/C3:
	mkdir -p $(OBJDIR)/C3

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ -MD -MT '$@' -MF '$(patsubst $(OBJDIR)/%.o,$(OBJDIR)/%.d,$@)'

$(EXENAME): $(OBJDIR) $(OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

clean:
	if test -e $(EXENAME); then rm $(EXENAME); fi
	if test -d $(OBJDIR); then rm -r $(OBJDIR); fi
