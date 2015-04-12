objmap=$(patsubst %.c,bin/%.o,$(subst /,-,$(1)))
objects=$(call objmap,$(sources))
targets=$(patsubst %/main.c,bin/%,$(wildcard */main.c))
sources=$(wildcard */*.c)

all: $(targets)

define targetrule
$(1): $(filter $(1)-%,$(objects)) | bin
	cc -o $$@ $$^ $$(CFLAGS) $(shell cat $(notdir $(1))/ldflags 2>/dev/null)
endef
$(foreach target,$(targets),$(eval $(call targetrule,$(target))))

define objrule
$(call objmap,$(1)): $(1) | bin
	cc -c -o $$@ $$< $$(CFLAGS)
endef
$(foreach source,$(sources),$(eval $(call objrule,$(source))))

manpages=$(patsubst %.ronn,%,$(wildcard */*.ronn))
doc: README.md $(manpages)

%.1: %.1.ronn
	ronn -r $<

README.md: $(wildcard */*.ronn)
	cat `echo $^ | tac -s" "` > $@

bin:
	mkdir bin

clean:
	rm -rf bin
