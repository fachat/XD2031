uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

ifeq ($(uname_S),not)
$(error Unable to detect operating system)
endif

ifeq ($(uname_S),Linux)
SUPPORTED_OS = Yes
MAKEINDEX = makeindex
OPEN = xdg-open
endif

ifeq ($(uname_S),Darwin)
SUPPORTED_OS = Yes
MAKEINDEX = MakeIndex
OPEN = open
endif

ifneq ($(SUPPORTED_OS),Yes)
$(warning $(uname_S) not yet supported, trying defaults)
MAKEINDEX = makeindex
OPEN = xdg-open
endif

all:	XD2031-Benutzerhandbuch.tex gpl-2.0.tex
	pdflatex XD2031-Benutzerhandbuch.tex
	$(MAKEINDEX) XD2031-Benutzerhandbuch.idx
	pdflatex XD2031-Benutzerhandbuch.tex
	# Move generated PDF to doc/
	mv XD2031-Benutzerhandbuch.pdf ../../doc/XD2031-Benutzerhandbuch.pdf
	# Display PDF
	$(OPEN) ../../doc/XD2031-Benutzerhandbuch.pdf

clean:
	rm -f *.aux *.fot *.glo *.idx *.ilg *.ind *.log *.out *.pdf *.toc
