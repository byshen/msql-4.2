TARGETS= all install clean

$(TARGETS) ::
	@if test ! -f src/site.mm					;\
	then								\
		echo ;							\
		echo "You need to run setup before make!";		\
		echo ;							\
		exit 1;							\
	fi

$(TARGETS) ::
	@cd src; $(MAKE) $@

very-clean : clean
	rm -f src/site.mm src/common/config.h src/common/config_extras.h
	rm -f config.cache config.log config.status conf/target

