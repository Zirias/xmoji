emojigen_MODULES=	emojigen \
			emojireader \
			groupnames \
			sourcegen \
			translate \
			util

ifeq ($(HAVE_CHAR32_T),1)
emojigen_DEFINES=	-DHAVE_CHAR32_T
endif

$(call binrules,emojigen)
