# gmake

NAME = z80test
VERSION = 1.0
PKG := $(NAME)-$(VERSION)

PROGS := z80full z80flags z80doc z80docflags z80ccf z80memptr
SRCS  := main idea crctab tests testmacros print

all: $(addsuffix .tap,$(PROGS))

.DELETE_ON_ERROR: %.out

%.out : %.asm $(addsuffix .asm,$(SRCS))
	sjasm $<

%.tap : loader.bas %.out
	mktap -b $(basename $(word 2,$^)) 10 <$(word 1,$^) >$@
	mktap    $(basename $(word 2,$^)) 32768 <$(word 2,$^) >>$@

FILES := Makefile loader.bas $(addsuffix .asm,$(PROGS)) $(addsuffix .asm, $(SRCS))

dist: all
	ln -s .. $(PKG)
	cp *.tap $(PKG)
	zip ../$(PKG).zip $(addprefix $(PKG)/src/, $(FILES)) $(PKG)/*.txt $(PKG)/*.tap
	rm $(PKG)/*.tap
	rm $(PKG)

clean:
	rm -rf *.out *.lst *.tap

tidy: clean
	rm -rf $(PROGS)
