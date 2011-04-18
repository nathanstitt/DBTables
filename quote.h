/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * $Id: quote.h 307 2004-07-14 15:12:38Z nas $ 
 * Copyright (C) 2002 Nathan Stitt  
 * See file COPYING for use and distribution permission.
 */

#ifndef _TMPLSQL_QUOTE_H_
#define _TMPLSQL_QUOTE_H_


#include <string>
#include <iostream>

extern "C" size_t PQescapeString(char *to, const char *from, size_t length);


	
//! Quote a value
/*! 
  Safely quote a value for use in a sql statement.  This is done as a template so that numeric values can not be quoted, but strings may.
		  
  The actual characters that are quoted may differ based on what RDMS is in use and it's quoting conventions.

  @param arg value to quote
  @return quoted value
*/
template<typename T>
inline
T quote( T arg,bool enclose = true ) {
	return arg;
}

//! specialization of quote to safely quote std::string
inline std::string quote( const std::string& arg,bool enclose = true ) {
	char *to = new char[ ( arg.length() * 2 ) + 1 ];
	PQescapeString ( to, arg.c_str(), arg.length()  );
	std::string ret_val;
	if ( enclose ) {
		ret_val = "'";
	}
	ret_val += to;
	if ( enclose ) {
		ret_val += "'";
	}
	delete [] to;
	return ret_val;
}

//! specialization of quote to safely quote const char*
inline std::string quote(  char* arg,bool enclose = true ) {
	char *to = new char[ ( strlen( arg ) * 2 ) + 1 ];
	PQescapeString ( to, arg,strlen( arg )  );
	std::string ret_val;
	if ( enclose ) {
		ret_val = "'";
	}
	ret_val += to;
	if ( enclose ) {
		ret_val += "'";
	}
	delete [] to;
	return ret_val;
}
 




#endif // _TMPLSQL_QUOTE_H_
