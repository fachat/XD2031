# List LaTeX files files here:
TEX_EN = gpl-2.0.tex
TEX_DE = XD2031-Benutzerhandbuch.tex

# Specify main document here without extension:
MAINDOC = XD2031-Benutzerhandbuch

###################################################################

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

all:    $(TEX_EN) $(MAINDOC).tex
	pdflatex $(MAINDOC).tex
	$(MAKEINDEX) $(MAINDOC).tex
	pdflatex $(MAINDOC).tex
	# Move generated PDF to doc/
	mv $(MAINDOC).pdf ../../doc/$(MAINDOC).pdf
	# Display PDF
	$(OPEN) ../../doc/$(MAINDOC).pdf

spellcheck:
ifneq ($(strip $(TEX_EN)),)
	hunspell -t -d en_US $(TEX_EN)
endif
ifneq ($(strip $(TEX_DE)),)
	hunspell -t -d de_DE $(TEX_DE)
endif

clean:
	rm -f *.aux *.fot *.glo *.idx *.ilg *.ind *.log *.out *.toc
