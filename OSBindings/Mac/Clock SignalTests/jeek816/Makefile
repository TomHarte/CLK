
F=suite-a

all: $(F).prg

$(F).prg:  $(F).asm
	acme --format plain -o $(F).prg -r $(F).r $(F).asm

clean:
	rm	-f $(F).prg
	rm	-f *~
