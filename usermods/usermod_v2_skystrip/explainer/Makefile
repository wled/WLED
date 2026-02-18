HTML := explainer.html

.PHONY: all clean open

all: $(HTML)

$(HTML): build.py template.html ../FAQ.md
	@echo [skystrip] build explainer
	python3 build.py

open: $(HTML)
	xdg-open $(HTML) || open $(HTML)

clean:
	rm -f $(HTML)

