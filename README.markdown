# DBTables

DBTables is a crude Object Relational Mapper (ORM) for Ruby and the Postgresql database.

## Some history:
  Way back in the dark ages of Ruby 1.6, there wasn't much choice if you wanted to access a database other than a buggy libpq library.  Out of frustration I wrote this which:

 * Reads in all the tables of a given database
 * Creates a Ruby class for each table with the same name
 * Reads each tables foreign keys and creates a method that will link to the other table's class.
 * Make a few helper methods to locate records by id, or by a give column, pretty much like find(id) and find_by_xxx in ActiveRecord.

Pretty nifty huh?  Unfortunately I never really got it very polished, and then ActiveRecord came out thus I quietly abandoned it in favor of ActiveRecord.  I did use it in a few projects though and was stable, just lacking in features.

I *think* it will still compile under ruby 1.8, but honestly haven't tested it for quite awhile (years).  If anyone wants to attempt to use it, I'll help out however I can.

## License
 * Source code is released under a do whatever the heck you want with it license. (Public Domain)

 
