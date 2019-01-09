.PHONY: all presentation.pdf clean

all: presentation.pdf

presentation.pdf: presentation.tex
	latexmk -pdf -pdflatex="pdflatex -interaction=nonstopmode" -use-make presentation.tex

clean:
	latexmk -C
