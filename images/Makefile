pngs = deps-basic.png deps-basic-clear.png deps-basic-clear-pipeline.png deps-basic-clear-pipeline-topbot.png deps-basic-clear-pipeline-all.png

all: $(pngs)

$(pngs): %.png: %.tex deps-common.tex
	pdflatex -shell-escape $<
