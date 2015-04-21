objmap=$(patsubst %.c,bin/%.o,$(subst /,-,$(1)))
objects=$(call objmap,$(sources))
libraries=$(patsubst lib%,bin/lib%.a,$(wildcard lib*))
targets=$(patsubst %/main.c,bin/%,$(wildcard */main.c))
sources=$(wildcard */*.c)
CFLAGS+=$(patsubst bin/%.a,-I%,$(libraries))

all: $(targets)

define targetrule
$(1): $(filter $(1)-%,$(objects)) $(libraries) | bin
	cc -o $$@ $$^ $(shell cat $(notdir $(1))/ldflags 2>/dev/null)
endef
$(foreach target,$(targets),$(eval $(call targetrule,$(target))))

define libraryrule
$(1): $(filter $(patsubst %.a,%,$(1))-%,$(objects)) | bin
	ar -rcs $$@ $$^
endef
$(foreach library,$(libraries),$(eval $(call libraryrule,$(library))))

define objrule
$(call objmap,$(1)): $(1) | bin submodules
	cc -c -o $$@ $$< $$(CFLAGS)
endef
$(foreach source,$(sources),$(eval $(call objrule,$(source))))

submodules: $(patsubst %,%/.git,$(wildcard lib*))
%/.git: %
	git submodule update --init $<

manpages=$(patsubst %.ronn,%,$(wildcard */*.ronn))
doc: README.md $(manpages)

%.1: %.1.ronn
	ronn -r $<

README.md: $(wildcard $(patsubst bin/%,%/*.ronn,$(targets)))
	cat `echo $^ | tac -s" "` > $@

bin:
	mkdir bin

clean:
	rm -rf bin
