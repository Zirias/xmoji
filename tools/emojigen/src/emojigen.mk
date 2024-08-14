emojigen_MODULES=	emojigen
ifeq ($(HAVE_CHAR32_T),1)
emojigen_DEFINES=	-DHAVE_CHAR32_T
endif
$(call binrules,emojigen)
