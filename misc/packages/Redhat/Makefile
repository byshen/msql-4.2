VERSION=3.2
RELEASE=RELEASE

FILE=msql-$(VERSION)
RPM_DIR=/usr/src/redhat

all: build


tarball:
	cd ../../../.. 						;\
	mv msql-30 msql-$(VERSION)				;\
	tar -cf $(RPM_DIR)/SOURCES/$(FILE).tar msql-$(VERSION)	;\
	mv msql-$(VERSION) msql-30

spec:
	sed "s,@VERSION@,$(VERSION)," < msql-3.spec | 		\
	sed "s,@RELEASE@,$(RELEASE)," |			 	\
	sed "s,@FILE@,$(FILE)," > $(RPM_DIR)/SPECS/msql-3.spec

build: tarball spec
	cd $(RPM_DIR)/SPECS					;\
	rpm -ba msql-3.spec
