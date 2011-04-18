/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * $Id: create.cc 1279 2005-10-17 16:30:57Z nas $ 
 * Copyright (C) 2002 Nathan Stitt  
 * See file COPYING for use and distribution permission.
 */

#include "dbtables.h"
#include "quote.h"
using namespace std;

//static const VALUE TIME_T = rb_eval_string("::Time");
static  VALUE RB_MONEY_T;
static  VALUE RB_DATE_T;
ID rb_method_to_s;
ID rb_method_class;
static const ID rb_method_superclass = rb_intern("superclass");

Table*
get_table( VALUE klass ) {
	tables_t::iterator it=Tables.find( klass );
	VALUE orig_klass = klass;
	while ( it == Tables.end()  && klass != rb_cObject && rb_respond_to( klass, rb_method_superclass ) ) {
		klass = rb_funcall( klass, rb_method_superclass, 0 );
		it=Tables.find( klass );
	}
	if ( it != Tables.end() ){
		return it->second;
	} else {
		std::string err( rb_class2name( orig_klass ) );
		err+=" nor it's ancestors were created by DBTables.";
		rb_raise( rb_eDBError, err.c_str() );
	}
}


VALUE
DBTables_each_field( VALUE self ){
	if (rb_block_given_p()) {
		for ( tables_t::const_iterator table = Tables.begin(); table != Tables.end(); ++table ){
			string str("DBTables::" + table->second->class_name );
			rb_yield( rb_eval_string( str.c_str() ) );
		}
	}
	return Qnil;
}

VALUE
DBTables_reset_connection( VALUE self ){
	db = rb_eval_string("NAS::DB.instance.conn");
	PGconn *conn = db_conn();
	PQreset(conn);
	if ( CONNECTION_OK == PQstatus(conn) ) {
		return Qtrue;
	} else {
		return Qfalse;
	}
}

void
add_method( Table *table, VALUE obj,  string nm, string type, bool an ) {
	Field f;
	f.type=type;
	f.allow_null=an;
	table->fields.insert( fields_t::value_type( nm, f ) );
	std::string eq( nm );
	eq += '=';
// 	if ( rb_respond_to( obj, rb_intern( nm.c_str() ) ) ){
// 		string newnm=nm+"_old";
// 		rb_alias( obj, rb_intern( newnm.c_str() ), rb_intern( nm.c_str() ) );
// 		rb_undef(obj, rb_intern( nm.c_str() ) );
// 	}
// 	if ( rb_respond_to( obj, rb_intern( eq.c_str() ) ) ){
// 		string neweq=nm+"_old=";
// 		rb_alias( obj, rb_intern( neweq.c_str() ), rb_intern( eq.c_str() ) );
// 		rb_undef(obj, rb_intern( eq.c_str() ) );
// 	}
	rb_define_method( obj, eq.c_str(),  RB_METHOD(DBTable_method_set), 1 );
	rb_define_method( obj, nm.c_str(),  RB_METHOD(DBTable_method_get), 0 );
}


VALUE
create_table( const string &name ){
 	Table *table=new Table( name );
	VALUE obj=DBTable_create( table->class_name );
	Tables.insert( tables_t::value_type( obj, table ) );
	stringstream sql;

	sql << "SELECT pg_attribute.attname "
	    << "FROM pg_class, pg_attribute, pg_index "
	    << "WHERE pg_class.oid = pg_attribute.attrelid AND "
	    << "pg_class.oid = pg_index.indrelid AND "
	    << "pg_index.indkey[0] = pg_attribute.attnum AND "
	    << "pg_index.indisprimary = 't' " 
	    << "AND pg_class.relname = '" 
	    << name << '\'';

 	PGconn *conn = db_conn();
	PGresult *res=PQexec( conn, sql.str().c_str() );
	if (!res || PQresultStatus(res) != PGRES_TUPLES_OK ) {
		sql << "\nFailed\n" << PQerrorMessage(conn);
		PQclear(res);
		rb_raise( rb_eDBError, sql.str().c_str() );
	}

	if  ( PQntuples( res ) ) {
		table->pk = PQgetvalue(res, 0, 0);
	} else {
		table->pk      = "oid";
		table->pk_type = "int4";
	}
	PQclear(res);
	sql.str("");
	sql << "select a.attname, t.typname, a.attnotnull, "
	    << "(select d.adsrc from pg_attrdef d where d.adrelid=c.oid and d.adnum=a.attnum) "
	    << "from pg_attribute a,pg_class c,pg_type t where a.attnum > 0 AND a.atttypid = t.oid "
	    << "and c.relname not like 'pg_%' and a.attrelid = c.oid and c.relname = '" << name << "'";
	res=PQexec( conn, sql.str().c_str() );
	if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
		sql << "\nFailed\n" << PQerrorMessage(conn);
		PQclear(res);
		rb_raise( rb_eDBError, sql.str().c_str() );
	}
	for (int i = 0; i < PQntuples(res); i++) {
		if ( table->pk == PQgetvalue(res, i, 0) ){
			table->pk_type   = PQgetvalue(res, i, 1 );
			string def = PQgetvalue(res, i, 3 );
			if ( def.find( "nextval" ) != string::npos ){
				table->pk_seq=def.substr( 9, def.find_first_of(  '\'',9 )-9 );
			}
		} else {
			bool nul=( (PQgetvalue(res, i, 2 ))[0]=='f' );
			add_method( table, obj, PQgetvalue(res, i, 0),PQgetvalue(res, i, 1 ),nul );
		}
	}
	PQclear(res);
	return obj;
}

void
make_tables(){
	stringstream sql;
	sql << "SELECT  c.relname AS tablename "
	    << "FROM pg_class c WHERE c.relkind='r' AND "
	    << "NOT EXISTS (SELECT 1 FROM pg_rewrite r WHERE r.ev_class = c.oid AND r.ev_type = '1')" // << " and c.relname = 'customers' "
	    << "AND c.relname NOT LIKE 'pg_%' ORDER BY relname";
 	PGconn *conn = db_conn();
	PGresult *res=PQexec( conn, sql.str().c_str() );
	if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
		sql << "\nFailed\n" << PQerrorMessage(conn);
		PQclear(res);
		rb_raise( rb_eDBError, sql.str().c_str() );
	}
	for (int i = 0; i < PQntuples(res); i++) {
		create_table( PQgetvalue(res, i, 0) );
	}
	PQclear(res);
}



extern "C"
void
Init_dbtables(){
	rb_require("nas/db");
	rb_require("nas/www/request");
	rb_require("nas/money/Money");
	rb_require("parsedate");
	rb_require("date");
	RB_MONEY_T = rb_path2class("Money");
	RB_DATE_T = rb_path2class("Date");
	rb_method_to_s = rb_intern("to_s");
	rb_method_class= rb_intern("class");
	db = rb_eval_string("NAS::DB.instance.conn");
 	rb_cDBTables = rb_define_module("DBTables");
	rb_cDBTable  = rb_define_class( "DBTable",rb_cObject );
	rb_eDBError  = rb_define_class("DBError", rb_eStandardError);
	make_tables();
	rb_define_singleton_method(rb_cDBTables, "each", RB_METHOD(DBTables_each_field), 0);
	rb_define_singleton_method(rb_cDBTables, "reset_connection", RB_METHOD(DBTables_reset_connection),0);

}

VALUE
make_date( char *value ){
	stringstream s;
	bool time=true;
	for ( unsigned int x = 0; x < strlen(value); ++x ){
		if ( ( value[x] < 48 || value[x] > 57 ) && value[x] != 46 ) {
			s << "   d=ParseDate::parsedate( '" << value << "',true )\n"
			  << "   Time.local( d[0],d[1], d[2], d[3], d[4], d[5] )";
			time=false;
			break;
		}
	}
	if ( time ){
		s << "   Time.at(" << value << ")";
	}
	return rb_eval_string(s.str().c_str());
}


bool
ensure_compatible_types( const std::string &field, const std::string &type, VALUE val ){
	if ( NIL_P( val ) ){
		return true;
	} else if ( ( type == "text" || type=="bpchar" ) && TYPE(val) == T_STRING ){
		return true;
	} else if ( ( type == "numeric" || type == "int4" || type == "int8" || type == "int2" || type == "float4" )
		    && 
		    ( TYPE(val) == T_FIXNUM || TYPE(val) == T_BIGNUM || TYPE(val) == T_FLOAT ) ) {
		return true;
	} else if ( type == "bool" && ( TYPE(val) == T_TRUE || TYPE(val) == T_FALSE ) ) {
		return true;
	} else if ( ( type == "timestamp" || type == "date" ) && ( rb_obj_is_kind_of( val,rb_cTime ) || rb_obj_is_kind_of( val,RB_DATE_T ) ) ) {
		return true;
	} else if ( ( type == "numeric" || type == "money" ) && rb_obj_is_kind_of( val,RB_MONEY_T ) ){
		return true;
	}
	stringstream msg;
	msg << "For field (" << field << ") DB Type (" << type << ") and Ruby type (" << STR2CSTR( rb_funcall( rb_funcall( val, rb_method_class,0 ), rb_method_to_s, 0 ) ) << ") are not compatible";
	rb_raise( rb_eDBError, msg.str().c_str() ); 
}

VALUE
to_rb_value( const std::string &type, char *value ){
	size_t len = strlen( value );
	if ( type == "text" || type == "bpchar" ){
		return rb_str_new2( value );
	} else if ( type == "numeric" || type == "money" ) {
		string s="Money.new(";
		s+=value;
		s+=")";
		return rb_eval_string( s.c_str() );
	} else if ( type == "int4" ) {
		return INT2FIX( atoi( value ) );
	} else if ( type == "bool" ) {
		if ( value[len-1] == 't' ){
			return Qtrue;
		} else {
			return Qfalse;
		}
	} else if ( type == "timestamp" ) {
		return make_date( value );
	} else if ( type == "date") {
		return make_date( value );
	} else if ( type == "float4" ){
		return rb_float_new( atof( value ) );
	} else if ( type == "int8" ){
		return INT2NUM( atoll( value ) );
	} else if ( type == "int2" ) {
		return INT2FIX( atoi( value ) );
	};
	stringstream msg;
	msg << "Unknown DB type of: " << type;
	rb_raise( rb_eDBError, msg.str().c_str() );
}




ostream&
from_rb_value( VALUE val, ostream& str ){
	switch (TYPE(val)){ 
	case T_STRING:
		str << quote( STR2CSTR( val ) );
		return str;
	case T_FIXNUM:
		str << FIX2INT( val );
		return str;
	case T_FLOAT:
		str << RFLOAT(val)->value;
		return str;
	case T_BIGNUM:
		str << INT2NUM( val );
		return str;
	case T_NIL:
		str << "null";
		return str;
	case T_TRUE:
		str << "true";
		return str;
	case T_FALSE:
		str << "false";
		return str;
	};
	
	if ( ( rb_obj_is_kind_of( val,RB_MONEY_T ) == Qtrue ) || ( rb_obj_is_kind_of( val,RB_DATE_T ) == Qtrue ) || ( rb_obj_is_kind_of( val,rb_cTime ) == Qtrue ) ){
		str << quote( STR2CSTR( rb_funcall( val, rb_method_to_s, 0 ) ) );
		return str;
	}


	stringstream msg;
	msg << "Sadly, I lack the ability to convert " << rb_class2name(val);
	rb_raise( rb_eDBError, msg.str().c_str() );
}

std::ostream &
operator << ( std::ostream& out_stream, RBval val ){
	return from_rb_value( val.val, out_stream );
}


PGconn*
db_conn(){
	PGconn *conn;
 	Data_Get_Struct(db, PGconn, conn);
 	if ( PQstatus(conn) != CONNECTION_OK ){
 		PQreset(conn);
 		if ( PQstatus(conn) != CONNECTION_OK ){
 			rb_raise( rb_eDBError, "DB Connection aborted and could not be restored" );
 		}
 	}
	return conn;
}
