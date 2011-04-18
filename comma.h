/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * $Id: comma.h 307 2004-07-14 15:12:38Z nas $ 
 * Copyright (C) 2001 Nathan Stitt  
 * See file COPYING for use and distribution permission.
 */

#ifndef _TMPLSQL_COMMAS_H_
#define _TMPLSQL_COMMAS_H_

#include <string>
#include <iostream>


	class Comma {
	public:
		//! ctor
		Comma();
		//! return a comma unless it is the first time used
		char get();
		//! return number of times get was called
		int times_called() const;
		//!  reset times_called to 0
		void reset();
		int operator++();
		int operator++(int);
	private:
		int times_called_;
	};


std::ostream & operator << ( std::ostream& out_stream, Comma& comma );

#endif // _TMPLSQL_COMMAS_H_

