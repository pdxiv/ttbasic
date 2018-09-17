BINARYNAME := ttbasic
BINARYENDING	:=
ifeq ($(OS),Windows_NT)
	BINARYENDING = .exe
endif

all: clean
	gcc main.c -o $(BINARYNAME)$(BINARYENDING)

.PHONY: clean
clean:
	rm -f $(BINARYNAME)$(BINARYENDING)
