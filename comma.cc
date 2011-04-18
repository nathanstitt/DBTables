/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * $Id: comma.cc 307 2004-07-14 15:12:38Z nas $ 
 * Copyright (C) 2001 Nathan Stitt  
 * See file COPYING for use and distribution permission.
 */


#include <string>
#include "comma.h"



Comma::Comma(){
	times_called_=0;
}

char
Comma::get(){
	if ( times_called_++ )
		return ',';
	else 
		return ' ';

}

int
Comma::times_called() const {
	return times_called_;
}

void
Comma::reset(){
	times_called_=0;
}

int
Comma::operator++(){
	return ++times_called_;
}


int
Comma::operator++(int){
	return times_called_++;
}

std::ostream &
operator << ( std::ostream& out_stream, Comma &comma ){
	out_stream << comma.get();
        return out_stream;
}
