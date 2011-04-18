/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * $Id: dbtables.h 582 2005-02-24 16:21:10Z nas $ 
 * Copyright (C) 2002 Nathan Stitt  
 * See file COPYING for use and distribution permission.
 */



#include <ruby.h>
#include <iostream>
#include <string>
#include <sstream>
#include <ext/hash_map>
#include <postgresql/libpq-fe.h>
#include <ctype.h>

#define RB_METHOD(func) ((VALUE (*)(...))func)

extern VALUE rb_cDBTables;
extern VALUE rb_cDBTable;
extern VALUE rb_eDBError;
extern VALUE db;
extern ID rb_method_to_s;
extern ID rb_method_class;
namespace gnu = __gnu_cxx;

struct strhash { gnu::hash<const char*> h;	inline size_t operator()(const std::string& key) const { return   h( key.c_str() ); } };

struct RBval{
	VALUE val;
	RBval( VALUE v ): val(v){}
};

std::ostream & operator << ( std::ostream& out_stream, RBval val );

VALUE to_rb_value( const std::string &type, char *value );

std::ostream& from_rb_value( VALUE, std::ostream& );
PGconn* db_conn();

VALUE DBTable_create( const std::string &name );
bool ensure_compatible_types( const std::string &field, const std::string &type, VALUE val );
struct Field {
	std::string type;
	bool allow_null;
};

typedef gnu::hash_map<std::string,Field,strhash> fields_t;

struct Table {
	Table( const std::string &n ) : db_name( n ), class_name( n ){
		class_name[0] = toupper( class_name[0] );
		std::string::size_type pos = 0;
		while ( pos != std::string::npos ){
			pos = class_name.find_first_of('_', pos+1 );
			if ( class_name.size() > pos ){
				std::string s;
				s=toupper( class_name[pos+1] );
				class_name.replace( pos, 2, s );
			}
		}
	}
	std::string pk;
	std::string pk_type;
	std::string pk_seq;
	std::string db_name;
	std::string class_name;
	fields_t fields;
};


typedef gnu::hash_map<VALUE,Table*> tables_t;

static tables_t Tables;

Table* get_table( VALUE klass );

VALUE DBTable_method_get( VALUE klass );
VALUE DBTable_pk_get( VALUE klass );
VALUE DBTable_method_set( VALUE klass, VALUE val );
VALUE DBTable_new( VALUE klass, VALUE val );
VALUE DBTable_init( VALUE klass, VALUE val );
