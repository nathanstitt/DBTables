
require 'mkmf'

create_makefile("dbtables")

`perl -pi -e "s/gcc/g\+\+/g" Makefile`
 



