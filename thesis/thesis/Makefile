.PHONY: all thesis.pdf clean

all: thesis.pdf

thesis.pdf: thesis.tex
	latexmk -pdf -pdflatex="pdflatex -interaction=nonstopmode" -use-make thesis.tex

clean:
	latexmk -C
