#
# Dummy Makefile used with makegen.
#
# NOTE : The TOP variable must be set to reflect the top of the
# src tree (either relative or absolute)
#


TOP=.
TARGETS= all install clean 

#$(TARGETS) :: 
#	@if test ! -d lib 	;\
#	then			\
#		echo ;\
#		echo "You have not made the target directory!"; \
#		echo "Please read the INSTALL file"; \
#		echo ;\
#		exit 1;\
#	fi

$(TARGETS) :: Makefile.full
	@$(MAKE) -f Makefile.full $@

Makefile.full :: $(TOP)/site.mm Makefile.tmpl
	@$(TOP)/makegen/makegen $(TOP)

very_clean: clean
	rm -f config.cache config.log config.status
	rm -f site.mm site.mm.in
	rm -f lib/*
