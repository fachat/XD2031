all:	docdefs.tex howto-compile.tex
	pdflatex howto-compile.tex
	MakeIndex howto-compile.idx
	pdflatex howto-compile.tex
	# OS X:
	# open howto-compile.pdf
	# Linux:
	# xdg-open howto-compile.pdf

spellcheck:
	hunspell -t -d en_US howto-compile.tex

clean:
	rm -f *.aux *.fot *.glo *.idx *.ilg *.ind *.log *.out *.toc
