#ifdef _WIN32
#define do_int64 signed __int64
#else
#define do_int64 signed long long int
#endif

#include <ruby.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#ifndef _WIN32
#include <sys/time.h>
#endif

#define DO_ID_CONST_GET rb_intern("const_get")

#define RUBY_STRING(char_ptr) rb_str_new2(char_ptr)
#define TAINTED_STRING(name, length) rb_tainted_str_new(name, length)
#define CONST_GET(scope, constant) (rb_funcall(scope, DO_ID_CONST_GET, 1, rb_str_new2(constant)))
#define ORACLE_CLASS(klass, parent) (rb_define_class_under(mDO_Oracle, klass, parent))
#define DEBUG(value) data_objects_debug(value)
#define RUBY_CLASS(name) rb_const_get(rb_cObject, rb_intern(name))

#ifndef RSTRING_PTR
#define RSTRING_PTR(s) (RSTRING(s)->ptr)
#endif

#ifndef RSTRING_LEN
#define RSTRING_LEN(s) (RSTRING(s)->len)
#endif

#ifndef RARRAY_LEN
#define RARRAY_LEN(a) RARRAY(a)->len
#endif


// To store rb_intern values
static ID DO_ID_NEW;
static ID DO_ID_LOGGER;
static ID DO_ID_DEBUG;
static ID DO_ID_LEVEL;
static ID DO_ID_LOG;
static ID DO_ID_TO_S;
static ID DO_ID_RATIONAL;

static ID DO_ID_NAME;

static ID DO_ID_NUMBER;
static ID DO_ID_VARCHAR2;
static ID DO_ID_CHAR;
static ID DO_ID_DATE;
static ID DO_ID_TIMESTAMP;
static ID DO_ID_TIMESTAMP_TZ;
static ID DO_ID_TIMESTAMP_LTZ;
static ID DO_ID_CLOB;
static ID DO_ID_BLOB;
static ID DO_ID_LONG;
static ID DO_ID_RAW;
static ID DO_ID_LONG_RAW;
static ID DO_ID_BFILE;
static ID DO_ID_BINARY_FLOAT;
static ID DO_ID_BINARY_DOUBLE;

static ID DO_ID_TO_A;
static ID DO_ID_TO_I;
static ID DO_ID_TO_S;
static ID DO_ID_TO_F;

static ID DO_ID_UTC_OFFSET;
static ID DO_ID_ZONE;
static ID DO_ID_FULL_CONST_GET;

static ID DO_ID_PARSE;
static ID DO_ID_FETCH;
static ID DO_ID_TYPE;
static ID DO_ID_EXECUTE;
static ID DO_ID_EXEC;

static ID DO_ID_SELECT_STMT;
static ID DO_ID_COLUMN_METADATA;
static ID DO_ID_PRECISION;
static ID DO_ID_SCALE;
static ID DO_ID_BIND_PARAM;
static ID DO_ID_ELEM;
static ID DO_ID_READ;

static ID DO_ID_CLOSE;
static ID DO_ID_LOGOFF;

static VALUE mExtlib;
static VALUE mDO;
static VALUE cDO_Quoting;
static VALUE cDO_Connection;
static VALUE cDO_Command;
static VALUE cDO_Result;
static VALUE cDO_Reader;
static VALUE cDO_Logger;
static VALUE cDO_Logger_Message;

static VALUE rb_cDate;
static VALUE rb_cDateTime;
static VALUE rb_cBigDecimal;
static VALUE rb_cByteArray;

static VALUE cOCI8;
static VALUE cOCI8_Cursor;
static VALUE cOCI8_BLOB;
static VALUE cOCI8_CLOB;

static VALUE mDO_Oracle;
static VALUE cDO_OracleConnection;
static VALUE cDO_OracleCommand;
static VALUE cDO_OracleResult;
static VALUE cDO_OracleReader;

static VALUE eArgumentError;
static VALUE eDO_SQLError;
static VALUE eDO_ConnectionError;
static VALUE eDO_DataError;

static void data_objects_debug(VALUE connection, VALUE string, struct timeval* start) {
  struct timeval stop;
  VALUE message;

  gettimeofday(&stop, NULL);
  do_int64 duration = (stop.tv_sec - start->tv_sec) * 1000000 + stop.tv_usec - start->tv_usec;

  message = rb_funcall(cDO_Logger_Message, DO_ID_NEW, 3, string, rb_time_new(start->tv_sec, start->tv_usec), INT2NUM(duration));

  rb_funcall(connection, DO_ID_LOG, 1, message);
}


static char * get_uri_option(VALUE query_hash, const char* key) {
  VALUE query_value;
  char * value = NULL;

  if(!rb_obj_is_kind_of(query_hash, rb_cHash)) { return NULL; }

  query_value = rb_hash_aref(query_hash, RUBY_STRING(key));

  if (Qnil != query_value) {
    value = StringValuePtr(query_value);
  }

  return value;
}

// Implementation using C functions

static VALUE parse_date(VALUE r_value) {
  int year, month, day;

  if (rb_obj_class(r_value) == rb_cDate) {
    return r_value;
  } else if (rb_obj_class(r_value) == rb_cTime ||
             rb_obj_class(r_value) == rb_cDateTime) {
    year = NUM2INT(rb_funcall(r_value, rb_intern("year"), 0));
    month = NUM2INT(rb_funcall(r_value, rb_intern("month"), 0));
    day = NUM2INT(rb_funcall(r_value, rb_intern("day"), 0));
    return rb_funcall(rb_cDate, DO_ID_NEW, 3, INT2NUM(year), INT2NUM(month), INT2NUM(day));
  } else {
    // Something went terribly wrong
    rb_raise(eDO_DataError, "Couldn't parse date from class %s object", rb_obj_classname(r_value));
  }
}

// Implementation using C functions

static VALUE parse_date_time(VALUE r_value) {
  VALUE time_array;
  int year, month, day, hour, min, sec;
  VALUE zone;

  if (rb_obj_class(r_value) == rb_cDateTime) {
    return r_value;
  } else if (rb_obj_class(r_value) == rb_cTime) {
    time_array = rb_funcall(r_value, DO_ID_TO_A, 0);
    year = NUM2INT(rb_ary_entry(time_array, 5));
    month = NUM2INT(rb_ary_entry(time_array, 4));
    day = NUM2INT(rb_ary_entry(time_array, 3));
    hour = NUM2INT(rb_ary_entry(time_array, 2));
    min = NUM2INT(rb_ary_entry(time_array, 1));
    sec = NUM2INT(rb_ary_entry(time_array, 0));
    zone = rb_funcall(r_value, DO_ID_ZONE, 0 );

    return  rb_funcall(rb_cDateTime, DO_ID_NEW, 7, INT2NUM(year), INT2NUM(month), INT2NUM(day),
                                               INT2NUM(hour), INT2NUM(min), INT2NUM(sec), zone);
  } else {
    // Something went terribly wrong
    rb_raise(eDO_DataError, "Couldn't parse datetime from class %s object", rb_obj_classname(r_value));
  }

}

static VALUE parse_time(VALUE r_value) {
  if (rb_obj_class(r_value) == rb_cTime) {
    return r_value;
  } else {
    // Something went terribly wrong
    rb_raise(eDO_DataError, "Couldn't parse time from class %s object", rb_obj_classname(r_value));
  }
}

static VALUE parse_boolean(VALUE r_value) {
  if (TYPE(r_value) == T_FIXNUM || TYPE(r_value) == T_BIGNUM) {
    return NUM2INT(r_value) >= 1 ? Qtrue : Qfalse;
  } else if (TYPE(r_value) == T_STRING) {
    char value = NIL_P(r_value) || RSTRING_LEN(r_value) == 0 ? '\0' : *(RSTRING_PTR(r_value));
    return value == 'Y' || value == 'y' || value == 'T' || value == 't' ? Qtrue : Qfalse;
  } else {
    // Something went terribly wrong
    rb_raise(eDO_DataError, "Couldn't parse boolean from class %s object", rb_obj_classname(r_value));
  }
}

/* ===== Typecasting Functions ===== */

static VALUE infer_ruby_type(VALUE type, VALUE precision, VALUE scale) {
  ID type_id = SYM2ID(type);

  if (type_id == DO_ID_NUMBER)
    return scale != Qnil && NUM2INT(scale) == 0 ?
            (NUM2INT(precision) == 1 ? rb_cTrueClass : rb_cInteger) : rb_cBigDecimal;
  else if (type_id == DO_ID_VARCHAR2 || type_id == DO_ID_CHAR || type_id == DO_ID_CLOB || type_id == DO_ID_LONG)
    return rb_cString;
  else if (type_id == DO_ID_DATE)
    // return rb_cDateTime;
    // by default map DATE type to Time class as it is much faster than DateTime class
    return rb_cTime;
  else if (type_id == DO_ID_TIMESTAMP || type_id == DO_ID_TIMESTAMP_TZ || type_id == DO_ID_TIMESTAMP_LTZ)
    // return rb_cDateTime;
    // by default map TIMESTAMP type to Time class as it is much faster than DateTime class
    return rb_cTime;
  else if (type_id == DO_ID_BLOB || type_id == DO_ID_RAW || type_id == DO_ID_LONG_RAW || type_id == DO_ID_BFILE)
    return rb_cByteArray;
  else if (type_id == DO_ID_BINARY_FLOAT || type_id == DO_ID_BINARY_DOUBLE)
    return rb_cFloat;
  else
    return rb_cString;
}

static VALUE typecast(VALUE r_value, const VALUE type) {
  VALUE r_data;

  if (type == rb_cInteger) {
    return TYPE(r_value) == T_FIXNUM || TYPE(r_value) == T_BIGNUM ? r_value : rb_funcall(r_value, DO_ID_TO_I, 0);

  } else if (type == rb_cString) {
    if (TYPE(r_value) == T_STRING)
      return r_value;
    else if (rb_obj_class(r_value) == cOCI8_CLOB)
      return rb_funcall(r_value, DO_ID_READ, 0);
    else
      return rb_funcall(r_value, DO_ID_TO_S, 0);

  } else if (type == rb_cFloat) {
    return TYPE(r_value) == T_FLOAT ? r_value : rb_funcall(r_value, DO_ID_TO_F, 0);

  } else if (type == rb_cBigDecimal) {
    VALUE r_string = TYPE(r_value) == T_STRING ? r_value : rb_funcall(r_value, DO_ID_TO_S, 0);
    return rb_funcall(rb_cBigDecimal, DO_ID_NEW, 1, r_string);

  } else if (type == rb_cDate) {
    return parse_date(r_value);

  } else if (type == rb_cDateTime) {
    return parse_date_time(r_value);

  } else if (type == rb_cTime) {
    return parse_time(r_value);

  } else if (type == rb_cTrueClass) {
    return parse_boolean(r_value);

  } else if (type == rb_cByteArray) {
    if (rb_obj_class(r_value) == cOCI8_BLOB)
      r_data = rb_funcall(r_value, DO_ID_READ, 0);
    else
      r_data = r_value;
    return rb_funcall(rb_cByteArray, DO_ID_NEW, 1, r_data);

  } else if (type == rb_cClass) {
    return rb_funcall(mDO, DO_ID_FULL_CONST_GET, 1, r_value);

  } else if (type == rb_cNilClass) {
    return Qnil;

  } else {
    if (rb_obj_class(r_value) == cOCI8_CLOB)
      return rb_funcall(r_value, DO_ID_READ, 0);
    else
      return r_value;
  }

}

static VALUE typecast_bind_value(VALUE connection, VALUE r_value) {
  VALUE r_class = rb_obj_class(r_value);
  VALUE oci8_conn = rb_iv_get(connection, "@connection");
  // replace nil value with '' as otherwise OCI8 cannot get bind variable type
  // '' will be inserted as NULL by Oracle
  if (NIL_P(r_value))
    return RUBY_STRING("");
  else if (r_class == rb_cString)
    // if string is longer than 4000 characters then convert to CLOB
    return RSTRING_LEN(r_value) <= 4000 ? r_value : rb_funcall(cOCI8_CLOB, DO_ID_NEW, 2, oci8_conn, r_value);
  else if (r_class == rb_cBigDecimal)
    return rb_funcall(r_value, DO_ID_TO_S, 1, RUBY_STRING("F"));
  else if (r_class == rb_cTrueClass)
    return INT2NUM(1);
  else if (r_class == rb_cFalseClass)
    return INT2NUM(0);
  else if (r_class == rb_cByteArray)
    return rb_funcall(cOCI8_BLOB, DO_ID_NEW, 2, oci8_conn, r_value);
  else if (r_class == rb_cClass)
    return rb_funcall(r_value, DO_ID_TO_S, 0);
  else
    return r_value;
}

/* ====== Public API ======= */
static VALUE cDO_OracleConnection_dispose(VALUE self) {
  VALUE oci8_conn = rb_iv_get(self, "@connection");

  if (Qnil == oci8_conn)
    return Qfalse;

  rb_funcall(oci8_conn, DO_ID_LOGOFF, 0);

  rb_iv_set(self, "@connection", Qnil);

  return Qtrue;
}

static VALUE cDO_OracleCommand_set_types(int argc, VALUE *argv, VALUE self) {
  VALUE type_strings = rb_ary_new();
  VALUE array = rb_ary_new();

  int i, j;

  for ( i = 0; i < argc; i++) {
    rb_ary_push(array, argv[i]);
  }

  for (i = 0; i < RARRAY_LEN(array); i++) {
    VALUE entry = rb_ary_entry(array, i);
    if(TYPE(entry) == T_CLASS) {
      rb_ary_push(type_strings, entry);
    } else if (TYPE(entry) == T_ARRAY) {
      for (j = 0; j < RARRAY_LEN(entry); j++) {
        VALUE sub_entry = rb_ary_entry(entry, j);
        if(TYPE(sub_entry) == T_CLASS) {
          rb_ary_push(type_strings, sub_entry);
        } else {
          rb_raise(eArgumentError, "Invalid type given");
        }
      }
    } else {
      rb_raise(eArgumentError, "Invalid type given");
    }
  }

  rb_iv_set(self, "@field_types", type_strings);

  return array;
}

typedef struct {
  VALUE self;
  VALUE connection;
  VALUE cursor;
  VALUE statement_type;
  VALUE args;
  VALUE sql;
  struct timeval start;
} cDO_OracleCommand_execute_try_t;

static VALUE cDO_OracleCommand_execute_try(cDO_OracleCommand_execute_try_t *arg);
static VALUE cDO_OracleCommand_execute_ensure(cDO_OracleCommand_execute_try_t *arg);

// called by Command#execute that is written in Ruby
static VALUE cDO_OracleCommand_execute_internal(VALUE self, VALUE connection, VALUE sql, VALUE args) {
  cDO_OracleCommand_execute_try_t arg;
  arg.self = self;
  arg.connection = connection;
  arg.sql = sql;
  // store start time before SQL parsing
  VALUE oci8_conn = rb_iv_get(connection, "@connection");
  if (Qnil == oci8_conn) {
    rb_raise(eDO_ConnectionError, "This connection has already been closed.");
  }
  gettimeofday(&arg.start, NULL);
  arg.cursor = rb_funcall(oci8_conn, DO_ID_PARSE, 1, sql);
  arg.statement_type = rb_funcall(arg.cursor, DO_ID_TYPE, 0);
  arg.args = args;

  return rb_ensure(cDO_OracleCommand_execute_try, (VALUE)&arg, cDO_OracleCommand_execute_ensure, (VALUE)&arg);
}

// wrapper for simple SQL calls without arguments
static VALUE execute_sql(VALUE connection, VALUE sql) {
  return cDO_OracleCommand_execute_internal(Qnil, connection, sql, Qnil);
}

static VALUE cDO_OracleCommand_execute_try(cDO_OracleCommand_execute_try_t *arg) {
  VALUE result = Qnil;
  int insert_id_present;

  // no arguments given
  if(NIL_P(arg->args)) {
    result = rb_funcall(arg->cursor, DO_ID_EXEC, 0);
  // arguments given - need to typecast
  } else {
    insert_id_present = (!NIL_P(arg->self) && rb_iv_get(arg->self, "@insert_id_present") == Qtrue);

    if (insert_id_present)
      rb_funcall(arg->cursor, DO_ID_BIND_PARAM, 2, RUBY_STRING(":insert_id"), INT2NUM(0));

    int i;
    VALUE r_orig_value, r_new_value;
    for (i = 0; i < RARRAY_LEN(arg->args); i++) {
      r_orig_value = rb_ary_entry(arg->args, i);
      r_new_value = typecast_bind_value(arg->connection, r_orig_value);
      if (r_orig_value != r_new_value)
        rb_ary_store(arg->args, i, r_new_value);
    }

    result = rb_apply(arg->cursor, DO_ID_EXEC, arg->args);

    if (insert_id_present) {
      VALUE insert_id = rb_funcall(arg->cursor, DO_ID_ELEM, 1, RUBY_STRING(":insert_id"));
      rb_iv_set(arg->self, "@insert_id", insert_id);
    }
  }

  if (SYM2ID(arg->statement_type) == DO_ID_SELECT_STMT)
    return arg->cursor;
  else {
    return result;
  }

}

static VALUE cDO_OracleCommand_execute_ensure(cDO_OracleCommand_execute_try_t *arg) {
  if (SYM2ID(arg->statement_type) != DO_ID_SELECT_STMT)
    rb_funcall(arg->cursor, DO_ID_CLOSE, 0);
  // Log SQL and execution time
  data_objects_debug(arg->connection, arg->sql, &(arg->start));
  return Qnil;
}

static VALUE cDO_OracleConnection_initialize(VALUE self, VALUE uri) {
  VALUE r_host, r_port, r_path, r_user, r_password;
  VALUE r_query, r_time_zone;
  char *non_blocking = NULL;
  char *time_zone = NULL;
  char set_time_zone_command[80];

  char *host = "localhost", *port = "1521", *path = NULL;
  char *connect_string;
  long connect_string_length;
  VALUE oci8_conn;

  r_user = rb_funcall(uri, rb_intern("user"), 0);
  r_password = rb_funcall(uri, rb_intern("password"), 0);

  r_host = rb_funcall(uri, rb_intern("host"), 0);
  if ( Qnil != r_host && RSTRING_LEN(r_host) > 0) {
    host = StringValuePtr(r_host);
  }

  r_port = rb_funcall(uri, rb_intern("port"), 0);
  if ( Qnil != r_port ) {
    r_port = rb_funcall(r_port, DO_ID_TO_S, 0);
    port = StringValuePtr(r_port);
  }

  r_path = rb_funcall(uri, rb_intern("path"), 0);
  if ( Qnil != r_path ) {
    path = StringValuePtr(r_path);
  }

  // If just host name is specified then use it as TNS names alias
  if ((r_host != Qnil && RSTRING_LEN(r_host) > 0) &&
      (r_port == Qnil) &&
      (r_path == Qnil || RSTRING_LEN(r_path) == 0)) {
    connect_string = host;
  // If database name is specified in path (in format "/database")
  } else if (path != NULL && strlen(path) > 1) {
    connect_string_length = strlen(host) + strlen(port) + strlen(path) + 4;
    connect_string = (char *)calloc(connect_string_length, sizeof(char));
    snprintf(connect_string, connect_string_length, "//%s:%s%s", host, port, path);
  } else {
    rb_raise(eDO_ConnectionError, "Database must be specified");
  }

  // oci8_conn = rb_funcall(cOCI8, DO_ID_NEW, 3, r_user, r_password, RUBY_STRING(connect_string));
  oci8_conn = rb_funcall(cDO_OracleConnection, rb_intern("oci8_new"), 3, r_user, r_password, RUBY_STRING(connect_string));

  // Pull the querystring off the URI
  r_query = rb_funcall(uri, rb_intern("query"), 0);

  non_blocking = get_uri_option(r_query, "non_blocking");
  // Enable non-blocking mode
  if (non_blocking != NULL && strcmp(non_blocking, "true") == 0)
    rb_funcall(oci8_conn, rb_intern("non_blocking="), 1, Qtrue);
  // By default enable auto-commit mode
  rb_funcall(oci8_conn, rb_intern("autocommit="), 1, Qtrue);
  // Set prefetch rows to 100 to increase fetching performance SELECTs with many rows
  rb_funcall(oci8_conn, rb_intern("prefetch_rows="), 1, INT2NUM(100));

  // Set session time zone
  // at first look for option in connection string
  time_zone = get_uri_option(r_query, "time_zone");

  rb_iv_set(self, "@uri", uri);
  rb_iv_set(self, "@connection", oci8_conn);

  // if no option specified then look in ENV['TZ']
  if (time_zone == NULL) {
    r_time_zone = rb_funcall(cDO_OracleConnection, rb_intern("ruby_time_zone"), 0);
    if (!NIL_P(r_time_zone))
      time_zone = StringValuePtr(r_time_zone);
  }
  if (time_zone) {
    snprintf(set_time_zone_command, 80, "alter session set time_zone = '%s'", time_zone);
    execute_sql(self, RUBY_STRING(set_time_zone_command));
  }

  execute_sql(self, RUBY_STRING("alter session set nls_date_format = 'YYYY-MM-DD HH24:MI:SS'"));
  execute_sql(self, RUBY_STRING("alter session set nls_timestamp_format = 'YYYY-MM-DD HH24:MI:SS.FF'"));
  execute_sql(self, RUBY_STRING("alter session set nls_timestamp_tz_format = 'YYYY-MM-DD HH24:MI:SS.FF TZH:TZM'"));

  return Qtrue;
}

static VALUE cDO_OracleCommand_execute_non_query(int argc, VALUE *argv[], VALUE self) {
  VALUE affected_rows = rb_funcall2(self, DO_ID_EXECUTE, argc, (VALUE *)argv);
  if (affected_rows == Qtrue)
    affected_rows = INT2NUM(0);

  VALUE insert_id = rb_iv_get(self, "@insert_id");

  return rb_funcall(cDO_OracleResult, DO_ID_NEW, 3, self, affected_rows, insert_id);
}

static VALUE cDO_OracleCommand_execute_reader(int argc, VALUE *argv[], VALUE self) {
  VALUE reader, query;
  VALUE field_names, field_types;
  VALUE column_metadata, column, column_name;

  int i;
  long field_count;
  int infer_types = 0;

  VALUE cursor = rb_funcall2(self, DO_ID_EXECUTE, argc, (VALUE *)argv);

  if (rb_obj_class(cursor) != cOCI8_Cursor) {
    rb_raise(eArgumentError, "\"%s\" is invalid SELECT query", StringValuePtr(query));
  }

  column_metadata = rb_funcall(cursor, DO_ID_COLUMN_METADATA, 0);
  field_count = RARRAY_LEN(column_metadata);
  // reduce field_count by 1 if RAW_RNUM_ is present as last column
  // (generated by DataMapper to simulate LIMIT and OFFSET)
  column = rb_ary_entry(column_metadata, field_count-1);
  column_name = rb_funcall(column, DO_ID_NAME, 0);
  if (strncmp(RSTRING_PTR(column_name), "RAW_RNUM_", RSTRING_LEN(column_name)) == 0)
    field_count--;

  reader = rb_funcall(cDO_OracleReader, DO_ID_NEW, 0);
  rb_iv_set(reader, "@reader", cursor);
  rb_iv_set(reader, "@field_count", INT2NUM(field_count));

  field_names = rb_ary_new();
  field_types = rb_iv_get(self, "@field_types");

  if ( field_types == Qnil || 0 == RARRAY_LEN(field_types) ) {
    field_types = rb_ary_new();
    infer_types = 1;
  } else if (RARRAY_LEN(field_types) != field_count) {
    // Whoops...  wrong number of types passed to set_types.  Close the reader and raise
    // and error
    rb_funcall(reader, DO_ID_CLOSE, 0);
    rb_raise(eArgumentError, "Field-count mismatch. Expected %ld fields, but the query yielded %ld", RARRAY_LEN(field_types), field_count);
  }

  for ( i = 0; i < field_count; i++ ) {
    column = rb_ary_entry(column_metadata, i);
    column_name = rb_funcall(column, DO_ID_NAME, 0);
    rb_ary_push(field_names, column_name);
    if ( infer_types == 1 ) {
      rb_ary_push(field_types,
        infer_ruby_type(rb_iv_get(column, "@data_type"),
          rb_funcall(column, DO_ID_PRECISION, 0),
          rb_funcall(column, DO_ID_SCALE, 0))
      );
    }
  }

  rb_iv_set(reader, "@position", INT2NUM(0));
  rb_iv_set(reader, "@fields", field_names);
  rb_iv_set(reader, "@field_types", field_types);

  rb_iv_set(reader, "@last_row", Qfalse);

  return reader;
}

static VALUE cDO_OracleReader_close(VALUE self) {
  VALUE cursor = rb_iv_get(self, "@reader");

  if (Qnil == cursor)
    return Qfalse;

  rb_funcall(cursor, DO_ID_CLOSE, 0);

  rb_iv_set(self, "@reader", Qnil);
  return Qtrue;
}

static VALUE cDO_OracleReader_next(VALUE self) {
  VALUE cursor = rb_iv_get(self, "@reader");

  int field_count;
  int i;

  if (Qnil == cursor || Qtrue == rb_iv_get(self, "@last_row"))
    return Qfalse;

  VALUE row = rb_ary_new();
  VALUE field_types, field_type;
  VALUE value;

  VALUE fetch_result = rb_funcall(cursor, DO_ID_FETCH, 0);

  if (Qnil == fetch_result) {
    rb_iv_set(self, "@values", Qnil);
    rb_iv_set(self, "@last_row", Qtrue);
    return Qfalse;
  }

  field_count = NUM2INT(rb_iv_get(self, "@field_count"));
  field_types = rb_iv_get(self, "@field_types");

  for ( i = 0; i < field_count; i++ ) {
    field_type = rb_ary_entry(field_types, i);
    value = rb_ary_entry(fetch_result, i);
    // Always return nil if the value returned from Oracle is null
    if (Qnil != value) {
      value = typecast(value, field_type);
    }

    rb_ary_push(row, value);
  }

  rb_iv_set(self, "@values", row);
  return Qtrue;
}

static VALUE cDO_OracleReader_values(VALUE self) {

  VALUE values = rb_iv_get(self, "@values");
  if(values == Qnil) {
    rb_raise(eDO_DataError, "Reader not initialized");
    return Qnil;
  } else {
    return values;
  }
}

static VALUE cDO_OracleReader_fields(VALUE self) {
  return rb_iv_get(self, "@fields");
}

static VALUE cDO_OracleReader_field_count(VALUE self) {
  return rb_iv_get(self, "@field_count");
}


void Init_do_oracle() {
  // rb_require("oci8");
  rb_require("date");
  rb_require("rational");
  rb_require("bigdecimal");
  rb_require("data_objects");

  // Get references classes needed for Date/Time parsing
  rb_cDate = CONST_GET(rb_mKernel, "Date");
  rb_cDateTime = CONST_GET(rb_mKernel, "DateTime");
  rb_cBigDecimal = CONST_GET(rb_mKernel, "BigDecimal");

  rb_funcall(rb_mKernel, rb_intern("require"), 1, rb_str_new2("data_objects"));

  DO_ID_NEW = rb_intern("new");
  DO_ID_LOGGER = rb_intern("logger");
  DO_ID_DEBUG = rb_intern("debug");
  DO_ID_LEVEL = rb_intern("level");
  DO_ID_LOG = rb_intern("log");
  DO_ID_TO_S = rb_intern("to_s");
  DO_ID_RATIONAL = rb_intern("Rational");

  DO_ID_NAME = rb_intern("name");

  DO_ID_NUMBER = rb_intern("number");
  DO_ID_VARCHAR2 = rb_intern("varchar2");
  DO_ID_CHAR = rb_intern("char");
  DO_ID_DATE = rb_intern("date");
  DO_ID_TIMESTAMP = rb_intern("timestamp");
  DO_ID_TIMESTAMP_TZ = rb_intern("timestamp_tz");
  DO_ID_TIMESTAMP_LTZ = rb_intern("timestamp_ltz");
  DO_ID_CLOB = rb_intern("clob");
  DO_ID_BLOB = rb_intern("blob");
  DO_ID_LONG = rb_intern("long");
  DO_ID_RAW = rb_intern("raw");
  DO_ID_LONG_RAW = rb_intern("long_raw");
  DO_ID_BFILE = rb_intern("bfile");
  DO_ID_BINARY_FLOAT = rb_intern("binary_float");
  DO_ID_BINARY_DOUBLE = rb_intern("binary_double");

  DO_ID_TO_A = rb_intern("to_a");
  DO_ID_TO_I = rb_intern("to_i");
  DO_ID_TO_S = rb_intern("to_s");
  DO_ID_TO_F = rb_intern("to_f");

  DO_ID_UTC_OFFSET = rb_intern("utc_offset");
  DO_ID_ZONE = rb_intern("zone");
  DO_ID_FULL_CONST_GET = rb_intern("full_const_get");

  DO_ID_PARSE = rb_intern("parse");
  DO_ID_FETCH = rb_intern("fetch");
  DO_ID_TYPE = rb_intern("type");
  DO_ID_EXECUTE = rb_intern("execute");
  DO_ID_EXEC = rb_intern("exec");

  DO_ID_SELECT_STMT = rb_intern("select_stmt");
  DO_ID_COLUMN_METADATA = rb_intern("column_metadata");
  DO_ID_PRECISION = rb_intern("precision");
  DO_ID_SCALE = rb_intern("scale");
  DO_ID_BIND_PARAM = rb_intern("bind_param");
  DO_ID_ELEM = rb_intern("[]");
  DO_ID_READ = rb_intern("read");

  DO_ID_CLOSE = rb_intern("close");
  DO_ID_LOGOFF = rb_intern("logoff");

  // Get references to the Extlib module
  mExtlib = CONST_GET(rb_mKernel, "Extlib");
  rb_cByteArray = CONST_GET(mExtlib, "ByteArray");

  // Get reference to OCI8 class
  cOCI8 = CONST_GET(rb_mKernel, "OCI8");
  cOCI8_Cursor = CONST_GET(cOCI8, "Cursor");
  cOCI8_BLOB = CONST_GET(cOCI8, "BLOB");
  cOCI8_CLOB = CONST_GET(cOCI8, "CLOB");

  // Get references to the DataObjects module and its classes
  mDO = CONST_GET(rb_mKernel, "DataObjects");
  cDO_Quoting = CONST_GET(mDO, "Quoting");
  cDO_Connection = CONST_GET(mDO, "Connection");
  cDO_Command = CONST_GET(mDO, "Command");
  cDO_Result = CONST_GET(mDO, "Result");
  cDO_Reader = CONST_GET(mDO, "Reader");
  cDO_Logger = CONST_GET(mDO, "Logger");
  cDO_Logger_Message = CONST_GET(cDO_Logger, "Message");

  // Top Level Module that all the classes live under
  mDO_Oracle = rb_define_module_under(mDO, "Oracle");

  eArgumentError = CONST_GET(rb_mKernel, "ArgumentError");
  eDO_SQLError = CONST_GET(mDO, "SQLError");
  eDO_ConnectionError = CONST_GET(mDO, "ConnectionError");
  eDO_DataError = CONST_GET(mDO, "DataError");
  // eOracleError = rb_define_class("OracleError", rb_eStandardError);

  cDO_OracleConnection = ORACLE_CLASS("Connection", cDO_Connection);
  rb_define_method(cDO_OracleConnection, "initialize", cDO_OracleConnection_initialize, 1);
  rb_define_method(cDO_OracleConnection, "dispose", cDO_OracleConnection_dispose, 0);

  cDO_OracleCommand = ORACLE_CLASS("Command", cDO_Command);
  rb_define_method(cDO_OracleCommand, "set_types", cDO_OracleCommand_set_types, -1);
  rb_define_method(cDO_OracleCommand, "execute_non_query", cDO_OracleCommand_execute_non_query, -1);
  rb_define_method(cDO_OracleCommand, "execute_reader", cDO_OracleCommand_execute_reader, -1);
  rb_define_method(cDO_OracleCommand, "execute_internal", cDO_OracleCommand_execute_internal, 3);

  cDO_OracleResult = ORACLE_CLASS("Result", cDO_Result);

  cDO_OracleReader = ORACLE_CLASS("Reader", cDO_Reader);
  rb_define_method(cDO_OracleReader, "close", cDO_OracleReader_close, 0);
  rb_define_method(cDO_OracleReader, "next!", cDO_OracleReader_next, 0);
  rb_define_method(cDO_OracleReader, "values", cDO_OracleReader_values, 0);
  rb_define_method(cDO_OracleReader, "fields", cDO_OracleReader_fields, 0);
  rb_define_method(cDO_OracleReader, "field_count", cDO_OracleReader_field_count, 0);

}
