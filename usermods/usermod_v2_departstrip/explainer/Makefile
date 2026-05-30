INSTALLS := spuyten oakstop temescal

HTMLS := $(INSTALLS:%=installs/%/explainer.html)
COMMON_TEMPLATES := common/template.html common/template_oakstop.html common/template_temescal.html
COMMON_FAQS := common/faq.md common/faq_quick.md

.PHONY: all clean FORCE $(INSTALLS:%=build-%) open-%

all: $(HTMLS)

define INSTALL_template
build-$(1): installs/$(1)/explainer.html

installs/$(1)/explainer.html: FORCE common/build.py $(COMMON_TEMPLATES) $(COMMON_FAQS) installs/$(1)/legend.json
	@echo [departstrip] build $(1)
	python3 common/build.py $(1)

open-$(1): build-$(1)
	xdg-open installs/$(1)/explainer.html || open installs/$(1)/explainer.html
endef

$(foreach inst,$(INSTALLS),$(eval $(call INSTALL_template,$(inst))))

clean:
	rm -f $(HTMLS)

FORCE:
	@true
