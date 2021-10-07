#!/bin/sh

. $MACRO_DIR/makegen/makegen.cf

src=$1
shift
comment=$*
base=`echo $src | sed "s/\.y\$//"`
outfile=`echo $src | sed "s/\.y\$/.c/"`
headfile=`echo $src | sed "s/\.y\$/.h/"`
obj=`echo $src | sed "s/\.y\$/.o/"`

echo	"$outfile : $src"
echo	"	@echo \"$comment\""
echo	'	$(YACC) $(YACC_FLAGS)'" $src"

echo    "	if test -f y.tab.c;\\"
echo    "	then\\"
echo	"		mv y.tab.c $outfile ;\\"
echo	"		mv y.tab.h $headfile ;\\"
echo	"	else\\"
echo	"		mv $base.tab.c $outfile ;\\"
echo	"		mv $base.tab.h $headfile ;\\"
echo    "	fi;\\"

echo
echo	"$obj : $outfile"
echo	'	$(CC) $(CC_FLAGS) -o '$obj' -c '$outfile
echo
echo	"clean ::"
echo	"	rm -f $outfile y.tab.* $outfile $headfile"
echo
