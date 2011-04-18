/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * $Id: dbtable.cc 677 2005-03-18 20:31:55Z nas $ 
 * Copyright (C) 2002 Nathan Stitt  
 * See file COPYING for use and distribution permission.
 */

#include <list>

#include "dbtables.h"
#include "quote.h"
#include "comma.h"

static void delete_table(Table *ptr){ } //delete ptr; 
static void mark_table(Table *storage ) { }

std::string field_str( const Table *t, bool with_id );

VALUE rb_cDBTables;
VALUE rb_cDBTable;
VALUE rb_eDBError;

VALUE db;

static const ID rb_method_has_key = rb_intern("has_key?");
static const ID rb_method_each = rb_intern("each");
static const ID rb_method_fetch = rb_intern("fetch");
static const ID rb_method_cmp = rb_intern("<=>");
static const ID rb_method_new = rb_intern("new");
using namespace std;


VALUE
DBTable_pk( VALUE klass ){
	Table *table;
	Data_Get_Struct( klass, Table, table );
	return rb_iv_get( klass, "pk" );
}

VALUE
DBTable_singleton_pk_name( VALUE klass ){
	Table *table = get_table( klass );
	stringstream s;
	s<< table->db_name << '.' << table->pk;
	return rb_str_new2( s.str().c_str() );
}

VALUE
DBTable_pk_name( VALUE klass ){
	Table *table;
	Data_Get_Struct( klass, Table, table );
	stringstream s;
	s<< table->db_name << '.' << table->pk;
	return rb_str_new2( s.str().c_str() );
}

VALUE
DBTable_has_pk( VALUE klass ){
	Table *table;
	Data_Get_Struct( klass, Table, table );
	if ( table->pk == "oid" ){
		return Qfalse;
	} else {
		return Qtrue;
	}
}

VALUE
DBTable_compare( VALUE klass, VALUE other ){
	if ( Qtrue == rb_obj_is_kind_of( other, rb_cDBTable ) ){
		Table *us, *them;
		Data_Get_Struct( klass, Table, us );
		Data_Get_Struct( klass, Table, them );
		if ( us->class_name == them->class_name ){
			int our_pk = FIX2INT(  rb_iv_get( klass, "pk" ) );
			int thier_pk = FIX2INT( rb_iv_get( other, "pk" ) );
			if ( our_pk == thier_pk ) {
				return Qtrue;
			} else {
				return Qfalse;
			}
		}
	}
	return Qfalse;
}

VALUE
DBTable_method_get( VALUE klass ){
	Table *table;
	Data_Get_Struct( klass, Table, table );
	char *name = rb_id2name(rb_frame_last_func());
	return rb_iv_get( klass, name );
}

VALUE
DBTable_method_set( VALUE klass, VALUE val ){
	std::string name( rb_id2name(rb_frame_last_func()) );
	name.resize( name.size()-1 );
	Table *table;
	Data_Get_Struct( klass, Table, table );
	fields_t::const_iterator field = table->fields.find( name );
	if ( field == table->fields.end() ) {
		rb_raise( rb_eDBError, "Field does not exist. Who am I? What am I doing here? This is not my house.." );
	}
	ensure_compatible_types( name, field->second.type, val );
	stringstream sql;
	sql << "update " << table->db_name << " set " << name << " = "  << RBval(val) << " where " << table->pk << " = " << RBval( rb_iv_get( klass, "pk" ) );
	PGconn *conn = db_conn();
	PGresult *res=PQexec( conn, sql.str().c_str() );
	if ( PQresultStatus(res) != PGRES_COMMAND_OK ){
		sql << "\nFailed\n" << PQerrorMessage(conn);;
		PQclear(res);
		rb_raise( rb_eDBError, sql.str().c_str() );
	}
	PQclear(res);
	rb_iv_set( klass, name.c_str(), val );
	return Qtrue;
}


VALUE
DBTable_new( VALUE klass, VALUE val ){
	VALUE args[]={ val };
	Table *table = get_table( klass );
	VALUE tobj = Data_Wrap_Struct(klass, mark_table, delete_table, table );
	rb_obj_call_init( tobj, 1, args );
	return tobj;
}

void
create_from_hash( Table *table, VALUE klass, VALUE val ){
	stringstream notfound,sql;
	Comma comma;
 	for ( fields_t::const_iterator f=table->fields.begin(); f != table->fields.end(); ++f ){
		VALUE v = rb_funcall( val, rb_method_fetch, 2, rb_str_new2(f->first.c_str()),Qnil );
		if ( ( ! f->second.allow_null ) && Qnil == v ) {
 			notfound << comma << f->first;
 		}
 	}
	comma.reset();
	if ( notfound.str().empty() ){
		sql << "insert into " << table->db_name << " (";
		for ( fields_t::const_iterator f=table->fields.begin(); f != table->fields.end(); ++f ){
			VALUE v = rb_funcall( val, rb_method_fetch,2, rb_str_new2( f->first.c_str() ), Qnil  );

			if ( Qnil != v ){
				sql << comma << f->first;
				ensure_compatible_types( f->first, f->second.type, v );
			}
		}
		sql << " ) values (";
		comma.reset();
		for ( fields_t::const_iterator f=table->fields.begin(); f != table->fields.end(); ++f ){
			VALUE v = rb_funcall( val, rb_method_fetch, 2, rb_str_new2(f->first.c_str()) ,Qnil );
			if ( Qnil != v ){
				sql << comma << RBval(v);
			}
			rb_iv_set( klass, f->first.c_str(), v );
		}
		sql << ")";
		PGconn *conn = db_conn();
		PGresult *res=PQexec( conn, sql.str().c_str() );
		if ( PQresultStatus(res) != PGRES_COMMAND_OK ){
			sql << "\nFailed\n" << PQerrorMessage(conn);
			PQclear(res);
			rb_raise( rb_eDBError, sql.str().c_str() );
		}
		if ( table->pk == "oid" ){
			rb_iv_set( klass, "pk", INT2FIX( PQoidValue( res ) ) );
		} else {
			PQclear(res);
			sql.str("");
			sql << "select currval('" << table->pk_seq << "')";
			res=PQexec( conn, sql.str().c_str() );
			if ( PQresultStatus(res) == PGRES_TUPLES_OK ){
				rb_iv_set( klass, "pk", to_rb_value( table->pk_type, PQgetvalue(res, 0, 0) ) );
			} else {
				PQclear(res);
				sql << "\nFailed. " <<  PQerrorMessage(conn);
				rb_raise( rb_eDBError, sql.str().c_str() );
			}
		}
		PQclear(res);
	} else {
		notfound << " were not set, but are not allowed to be null";
		rb_raise( rb_eDBError, notfound.str().c_str() );
	}
}



VALUE
pair_iterate( VALUE str, list<pair<string,string> > *cont ){
	cont->push_back( list<pair<string,string> >::value_type( STR2CSTR( rb_funcall( rb_ary_entry(str,0), rb_method_to_s, 0 ) ),
					     STR2CSTR( rb_funcall( rb_ary_entry(str,1), rb_method_to_s, 0 ) ) ) );
	return Qtrue;
}


	
VALUE
exec_query( VALUE klass, Table *table, string sql ){
	VALUE retarray = rb_ary_new();
 	PGconn *conn = db_conn();
	PGresult *res=PQexec( conn, sql.c_str() );
	for (int row = 0; row < PQntuples(res); ++row) {
		VALUE array = rb_ary_new2( table->fields.size()+1 );
		rb_ary_push( array, to_rb_value(table->pk_type, PQgetvalue(res, row, 0) ) );
		int fcount=1;
		for ( fields_t::const_iterator f=table->fields.begin(); f != table->fields.end(); ++f ){
			if ( PQgetisnull( res, row, fcount ) ){
				rb_ary_push( array, Qnil );
			} else {
				rb_ary_push( array, to_rb_value( f->second.type, PQgetvalue(res, row, fcount) ) );
			}
			++fcount;
		}
		VALUE newobj = rb_funcall( klass, rb_method_new, 1, array );
		if ( rb_block_given_p()) {
			rb_yield( newobj );
		} else {
			rb_ary_push( retarray, newobj );
		}
	}
	PQclear(res);
	if ( rb_block_given_p()) {
		return Qtrue;
	} else {
		return retarray;
	}
}

VALUE
DBTable_all(  int argc, VALUE *argv, VALUE klass ){
 	stringstream sql;
	Table *table = get_table( klass );
	VALUE orderby;
	sql << "select " << field_str( table, true ) << " from " << table->db_name;
	rb_scan_args(argc, argv, "01", &orderby);
	if (argc != 0 ){
		sql << " order by " <<  STR2CSTR( orderby );
	}
	return exec_query( klass, table, sql.str() );
}


VALUE
DBTable_matching_sql( VALUE klass, VALUE where ){
 	stringstream sql;
	Table *table = get_table( klass );
	sql << "select " << field_str( table, true ) << " from " << table->db_name << " where " << STR2CSTR( where );
	return exec_query( klass, table, sql.str() );
}


std::string
construct_hash_query( VALUE klass, VALUE val,Table *table ){
 	stringstream sql;
	Comma comma;
	VALUE (*m1)(long unsigned int);
	VALUE (*m2)(...);
	m1 = (VALUE (*)(long unsigned int))RB_METHOD(rb_each);
	m2 = (VALUE (*)(...))RB_METHOD(pair_iterate);
	list<pair<string,string> > finding_fields;

	rb_iterate(m1, val, m2, (VALUE)&finding_fields);
	sql << "select " << field_str( table, true ) << " from " << table->db_name << " where ";
	bool first = true;
	for ( list<pair<string,string> >::const_iterator field=finding_fields.begin(); field != finding_fields.end(); ++field ){
		if ( table->fields.count( field->first ) ){
			if ( ! first ) sql << " and ";
			sql << table->db_name << '.' << field->first << '=' << quote( field->second );
			if ( first ) first = false;
		}
	}
	return sql.str();
}

VALUE
DBTable_find_one_matching( VALUE klass, VALUE val ){
	Table *table = get_table( klass );
	std::string sql = construct_hash_query( klass, val,table );
	sql += " limit 1";
	VALUE array =exec_query( klass, table, sql );
	if ( RARRAY( array )->len )
		return  rb_ary_entry( array, 0 );
	else
		return Qnil;

}

VALUE
DBTable_matching( VALUE klass, VALUE val ){
	Table *table = get_table( klass );
	return exec_query( klass, table, construct_hash_query( klass, val,table ) );
}

VALUE
single_iterate( VALUE val, list<VALUE> *cont ){
	cont->push_back( val );
	return Qtrue;
}


void
set_from_array( Table *table, VALUE klass, VALUE array ){
	VALUE (*m1)(long unsigned int);
	VALUE (*m2)(...);
	m1 = (VALUE (*)(long unsigned int))RB_METHOD(rb_each);
	m2 = (VALUE (*)(...))RB_METHOD(single_iterate);
	list<VALUE> vals;
	rb_iterate(m1, array, m2, (VALUE)&vals);

	rb_iv_set( klass, "pk", vals.front() );
	vals.pop_front();
	for ( fields_t::const_iterator f=table->fields.begin(); f != table->fields.end(); ++f ){
		if ( vals.empty() ){
			rb_raise( rb_eDBError, "Insufficient array elements." );
		}
		VALUE val = vals.front();
		ensure_compatible_types( f->first, f->second.type, val );
		rb_iv_set( klass, f->first.c_str(), val );
		vals.pop_front();
	}
}

void
set_from_hash( Table *table, VALUE klass, VALUE val ){
	stringstream notfound;
	Comma comma;

	rb_iv_set( klass, "pk", rb_hash_aref( val, rb_str_new2( table->pk.c_str() ) ) );

 	for ( fields_t::const_iterator f=table->fields.begin(); f != table->fields.end(); ++f ){
		VALUE v = rb_funcall( val, rb_method_fetch,2, rb_str_new2(f->first.c_str()),Qnil );
		if ( ( ! f->second.allow_null ) && Qnil == v ) {
 			notfound << comma << f->first;
 		}
 	}
	if ( notfound.str().empty() ){
		for ( fields_t::const_iterator f=table->fields.begin(); f != table->fields.end(); ++f ){
			VALUE v = rb_funcall( val, rb_method_fetch,2, rb_str_new2(f->first.c_str()),Qnil );
			if ( Qnil != v ){
				ensure_compatible_types( f->first, f->second.type, v );
				rb_iv_set( klass, f->first.c_str(), v );
			}
		}
	} else {
		notfound << " were not set, but are not allowed to be null";
		rb_raise( rb_eDBError, notfound.str().c_str() );
	}
}

void
set_from_pk( Table *table, VALUE klass, VALUE val ){
	rb_iv_set( klass, "pk", val );
	stringstream sql;
	Comma comma;
	sql << "select " << field_str( table,false ) 
	    <<  " from " << table->db_name 
	    << " where " << table->pk << " = " 
	    <<  quote( STR2CSTR( rb_funcall( val, rb_method_to_s, 0 ) ) );

 	PGconn *conn = db_conn();
	PGresult *res=PQexec( conn, sql.str().c_str() );
	if (!res || PQresultStatus(res) != PGRES_TUPLES_OK  || !PQntuples( res ) ) {
		sql << "\nFailed\n" << PQerrorMessage(conn);
		PQclear(res);
		rb_raise( rb_eDBError, sql.str().c_str() );
	}
	if ( PQntuples( res ) != 1 ){
		sql << "\nFailed to return only one row";
		PQclear(res);
		rb_raise( rb_eDBError, sql.str().c_str() );
	}
	int i=0;
	for ( fields_t::const_iterator f=table->fields.begin(); f != table->fields.end(); ++f ){
		if ( PQgetisnull( res, 0, i ) ){
			rb_iv_set( klass, f->first.c_str(), Qnil );
		} else {
			rb_iv_set( klass, f->first.c_str(), to_rb_value( f->second.type, PQgetvalue(res, 0, i) ) );
		}
		++i;
	}
}

VALUE
DBTable_init( VALUE klass, VALUE val ){
	Table *table;

	Data_Get_Struct( klass, Table, table );
	if ( Qtrue == rb_respond_to( val, rb_method_has_key ) ) {
		if ( Qtrue == rb_funcall( val, rb_method_has_key, 1, rb_str_new2( table->pk.c_str() ) ) ) {
			// initialize from hash
			set_from_hash( table, klass, val );
		} else {
			// create row from hash
			create_from_hash( table, klass, val );	
		}
	} else if ( Qtrue == rb_respond_to( val, rb_method_each ) ) {
		// array initialize
		set_from_array( table, klass, val );
	} else {
		// id initialize
		set_from_pk( table, klass, val );
	}
	return klass;
}

std::string
db_representation( const std::string &table, const std::string &name, const Field &f ){
	std::string ret;
	if ( ( f.type == "timestamp" ) || ( f.type == "date" ) ){
		ret="extract(epoch from " + table;
		ret+='.' + name;
		ret+=") as " + name;
	} else {
		ret=table+'.';
		ret+=name;
	}
	return ret;
}

std::string
field_str( const Table *t, bool with_id ){
	std::stringstream str;
	Comma comma;
	if ( with_id ){
		str <<  t->db_name << '.' << t->pk;
		++comma;
	}
	for ( fields_t::const_iterator f=t->fields.begin(); f != t->fields.end(); ++f ){
		str << comma << db_representation( t->db_name, f->first, f->second );
	}
	return str.str();
}

VALUE
DBTable_singleton_table( VALUE klass ){
	Table *table = get_table( klass );
	return rb_str_new2( table->db_name.c_str() );
}

VALUE
DBTable_num_fields( VALUE klass ){
	Table *table = get_table( klass );
	return INT2FIX( table->fields.size()+1 );
}

VALUE
DBTable_table( VALUE klass ){
	Table *table;
	Data_Get_Struct( klass, Table, table );
	return rb_str_new2( table->db_name.c_str() );
}

VALUE
DBTable_singleton_fields( int argc, VALUE *argv, VALUE klass ){
	Table *table = get_table( klass );
	VALUE with_id;
	rb_scan_args(argc, argv, "01", &with_id);
	if (argc == 0 || with_id == Qfalse){
		return rb_str_new2( field_str( table,true ).c_str() );
	} else {
		return rb_str_new2( field_str( table,false ).c_str() );
	}
}

VALUE
DBTable_field_index( VALUE klass, VALUE field ){
	Table *t = get_table( klass );
	unsigned char index=1;
	for ( fields_t::const_iterator f=t->fields.begin(); f != t->fields.end(); ++f ){
		if  ( f->first == STR2CSTR( field ) ){
			return INT2FIX( index );
		}
		++index;
	}
	return Qnil;
}




VALUE
DBTable_fields_str( int argc, VALUE *argv, VALUE klass ){
	Table *table;
	Data_Get_Struct( klass, Table, table );
	VALUE with_id;
	rb_scan_args(argc, argv, "01", &with_id);
	if (argc == 0 || with_id == Qfalse){
		return rb_str_new2( field_str( table,true ).c_str() );
	} else {
		return rb_str_new2( field_str( table,false ).c_str() );
	}
}

VALUE
DBTable_fields( VALUE klass ){
	if (rb_block_given_p()) {
		Table *table;
		Data_Get_Struct( klass, Table, table );
		rb_yield( rb_str_new2( table->pk.c_str() ) );
		for ( fields_t::const_iterator f=table->fields.begin(); f != table->fields.end(); ++f ){
			rb_yield( rb_str_new2( f->first.c_str() ) );
		}
		return Qtrue;		
	} else {
		return DBTable_fields_str( 0, 0, klass );
	}
}




VALUE
DBTable_allow_null( VALUE klass, VALUE val ){
	Table *table;
	Data_Get_Struct( klass, Table, table );
	fields_t::const_iterator f=table->fields.find( STR2CSTR( val ) );
	if ( ( f == table->fields.end() ) || ( f->second.allow_null ) ) {
		return Qtrue;
	} else {
		return Qfalse;
	}
}

VALUE
DBTable_create( const std::string &name ){
	VALUE obj = rb_define_class_under( rb_cDBTables, name.c_str(),rb_cDBTable );

	rb_define_singleton_method(obj, "new", RB_METHOD(DBTable_new),1);
	rb_define_singleton_method(obj, "fields", RB_METHOD(DBTable_singleton_fields),-1);
	rb_define_singleton_method(obj, "table", RB_METHOD(DBTable_singleton_table),0);
	rb_define_singleton_method(obj, "pk_name", RB_METHOD(DBTable_singleton_pk_name),0);
	rb_define_singleton_method(obj, "matching", RB_METHOD(DBTable_matching),1);
	rb_define_singleton_method(obj, "find_one_matching", RB_METHOD(DBTable_find_one_matching),1);
	rb_define_singleton_method(obj, "all", RB_METHOD(DBTable_all),-1);
	rb_define_singleton_method(obj, "each", RB_METHOD(DBTable_all),-1);
	rb_define_singleton_method(obj, "matching_sql", RB_METHOD(DBTable_matching_sql),1);
	rb_define_singleton_method(obj, "num_fields", RB_METHOD(DBTable_num_fields),0);
	rb_define_singleton_method(obj, "field_index", RB_METHOD(DBTable_field_index),1);

	rb_define_method(obj, "table", RB_METHOD(DBTable_table),0);
	rb_define_method(obj, "each", RB_METHOD(DBTable_fields),0);
	rb_define_method(obj, "initialize", RB_METHOD(DBTable_init), 1);
	rb_define_method(obj, "allow_null?", RB_METHOD(DBTable_allow_null), 1);
	rb_define_method(obj, "fields", RB_METHOD(DBTable_fields), 0 );
	rb_define_method(obj, "pk", RB_METHOD(DBTable_pk), 0);
	rb_define_method(obj, "has_pk?", RB_METHOD(DBTable_has_pk), 0);
	rb_define_method(obj, "pk_name", RB_METHOD(DBTable_pk_name), 0);
	rb_define_method(obj, "==", RB_METHOD(DBTable_compare), 1);
	rb_define_method(obj, "fields_str", RB_METHOD(DBTable_fields_str), -1 );

	rb_define_alias(obj, "db_pk", "pk");

	rb_include_module( obj,rb_mComparable );
	rb_include_module( obj, rb_mEnumerable );

	return obj;
}
