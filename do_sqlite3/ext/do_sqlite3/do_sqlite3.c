#include "do_sqlite3.h"
#include "error.h"

#include "do_common.h"

VALUE mSqlite3;
VALUE cConnection;
VALUE cCommand;
VALUE cResult;
VALUE cReader;

VALUE OPEN_FLAG_READONLY;
VALUE OPEN_FLAG_READWRITE;
VALUE OPEN_FLAG_CREATE;
VALUE OPEN_FLAG_NO_MUTEX;
VALUE OPEN_FLAG_FULL_MUTEX;

void raise_error(VALUE self, sqlite3 *result, VALUE query) {
  int errnum = sqlite3_errcode(result);
  const char *message = sqlite3_errmsg(result);
  VALUE sql_state = rb_str_new2("");

  do_raise_error(self, errors, errnum, message, query, sql_state);
}

VALUE typecast(sqlite3_stmt *stmt, int i, VALUE type, int encoding) {
  int original_type = sqlite3_column_type(stmt, i);
  int length = sqlite3_column_bytes(stmt, i);

  if (original_type == SQLITE_NULL) {
    return Qnil;
  }

#ifdef HAVE_RUBY_ENCODING_H
  rb_encoding *internal_encoding = rb_default_internal_encoding();
#else
  void *internal_encoding = NULL;
#endif

  if (type == Qnil) {
    switch (original_type) {
      case SQLITE_INTEGER:
        type = rb_cInteger;
        break;

      case SQLITE_FLOAT:
        type = rb_cFloat;
        break;

      case SQLITE_BLOB:
        type = rb_cByteArray;
        break;

      default:
        type = rb_cString;
        break;
    }
  }

  if (type == rb_cInteger) {
    return LL2NUM(sqlite3_column_int64(stmt, i));
  }
  else if (type == rb_cString) {
    return DO_STR_NEW((char*)sqlite3_column_text(stmt, i), length, encoding, internal_encoding);
  }
  else if (type == rb_cFloat) {
    return rb_float_new(sqlite3_column_double(stmt, i));
  }
  else if (type == rb_cBigDecimal) {
    return rb_funcall(rb_cBigDecimal, ID_NEW, 1, rb_str_new((char*)sqlite3_column_text(stmt, i), length));
  }
  else if (type == rb_cDate) {
    return parse_date((char*)sqlite3_column_text(stmt, i));
  }
  else if (type == rb_cDateTime) {
    return parse_date_time((char*)sqlite3_column_text(stmt, i));
  }
  else if (type == rb_cTime) {
    return parse_time((char*)sqlite3_column_text(stmt, i));
  }
  else if (type == rb_cTrueClass) {
    return strcmp((char*)sqlite3_column_text(stmt, i), "t") == 0 ? Qtrue : Qfalse;
  }
  else if (type == rb_cByteArray) {
    return rb_funcall(rb_cByteArray, ID_NEW, 1, rb_str_new((char*)sqlite3_column_blob(stmt, i), length));
  }
  else if (type == rb_cClass) {
    return rb_funcall(mDO, rb_intern("full_const_get"), 1, rb_str_new((char*)sqlite3_column_text(stmt, i), length));
  }
  else if (type == rb_cNilClass) {
    return Qnil;
  }
  else {
    return DO_STR_NEW((char*)sqlite3_column_text(stmt, i), length, encoding, internal_encoding);
  }
}

#ifdef HAVE_SQLITE3_OPEN_V2

#define FLAG_PRESENT(query_values, flag) !NIL_P(rb_hash_aref(query_values, flag))

int flags_from_uri(VALUE uri) {
  VALUE query_values = rb_funcall(uri, rb_intern("query"), 0);
  int flags = 0;

  if (!NIL_P(query_values) && TYPE(query_values) == T_HASH) {
    /// scan for flags
#ifdef SQLITE_OPEN_READONLY
    if (FLAG_PRESENT(query_values, OPEN_FLAG_READONLY)) {
      flags |= SQLITE_OPEN_READONLY;
    }
    else {
      flags |= SQLITE_OPEN_READWRITE;
    }
#endif

#ifdef SQLITE_OPEN_NOMUTEX
    if (FLAG_PRESENT(query_values, OPEN_FLAG_NO_MUTEX)) {
      flags |= SQLITE_OPEN_NOMUTEX;
    }
#endif

#ifdef SQLITE_OPEN_FULLMUTEX
    if (FLAG_PRESENT(query_values, OPEN_FLAG_FULL_MUTEX)) {
      flags |= SQLITE_OPEN_FULLMUTEX;
    }
#endif
    
    flags |= SQLITE_OPEN_CREATE;
  }
  else {
    flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
  }

  return flags;
}

#endif

/****** Public API ******/

VALUE cConnection_initialize(VALUE self, VALUE uri) {
  VALUE path = rb_funcall(uri, rb_intern("path"), 0);
  sqlite3 *db = NULL;
  int ret;

#ifdef HAVE_SQLITE3_OPEN_V2
  ret = sqlite3_open_v2(StringValuePtr(path), &db, flags_from_uri(uri), 0);
#else
  ret = sqlite3_open(StringValuePtr(path), &db);
#endif

  if (ret != SQLITE_OK) {
    raise_error(self, db, Qnil);
  }

  rb_iv_set(self, "@uri", uri);
  rb_iv_set(self, "@connection", Data_Wrap_Struct(rb_cObject, 0, 0, db));
  // Sqlite3 only supports UTF-8, so this is the standard encoding
  rb_iv_set(self, "@encoding", rb_str_new2("UTF-8"));
#ifdef HAVE_RUBY_ENCODING_H
  rb_iv_set(self, "@encoding_id", INT2FIX(rb_enc_find_index("UTF-8")));
#endif

  return Qtrue;
}

VALUE cConnection_dispose(VALUE self) {
  VALUE connection_container = rb_iv_get(self, "@connection");

  if (connection_container == Qnil) {
    return Qfalse;
  }

  sqlite3 *db = DATA_PTR(connection_container);

  if (!db) {
    return Qfalse;
  }

  sqlite3_close(db);
  rb_iv_set(self, "@connection", Qnil);
  return Qtrue;
}

VALUE cConnection_quote_boolean(VALUE self, VALUE value) {
  return rb_str_new2(value == Qtrue ? "'t'" : "'f'");
}

VALUE cConnection_quote_string(VALUE self, VALUE string) {
  const char *source = rb_str_ptr_readonly(string);

  // Wrap the escaped string in single-quotes, this is DO's convention
  char *escaped_with_quotes = sqlite3_mprintf("%Q", source);
  VALUE result = rb_str_new2(escaped_with_quotes);

#ifdef HAVE_RUBY_ENCODING_H
  rb_enc_associate_index(result, FIX2INT(rb_iv_get(self, "@encoding_id")));
#endif
  sqlite3_free(escaped_with_quotes);
  return result;
}

VALUE cConnection_quote_byte_array(VALUE self, VALUE string) {
  VALUE source = StringValue(string);
  VALUE array = rb_funcall(source, rb_intern("unpack"), 1, rb_str_new2("H*"));

  rb_ary_unshift(array, rb_str_new2("X'"));
  rb_ary_push(array, rb_str_new2("'"));
  return rb_ary_join(array, Qnil);
}

VALUE cConnection_enable_load_extension(VALUE self, VALUE value) {
  VALUE connection = rb_iv_get(self, "@connection");

  if (connection == Qnil) {
    return Qfalse;
  }

  sqlite3 *db = DATA_PTR(connection);

  if (!db) {
    return Qfalse;
  }

  int status = sqlite3_enable_load_extension(db, value == Qtrue ? 1 : 0);

  if (status != SQLITE_OK) {
    rb_raise(eConnectionError, "Error enabling load extension.");
  }
  return Qtrue;
}

VALUE cConnection_load_extension(VALUE self, VALUE string) {
  VALUE connection = rb_iv_get(self, "@connection");

  if (connection == Qnil) {
    return Qfalse;
  }

  sqlite3 *db = DATA_PTR(connection);

  if (!db) {
    return Qfalse;
  }

  const char *extension_name  = rb_str_ptr_readonly(string);
  char *errmsg = NULL;
  int status = sqlite3_load_extension(db, extension_name, 0, &errmsg);

  if (status != SQLITE_OK) {
    rb_raise(eConnectionError, "%s", errmsg);
  }
  return Qtrue;
}

VALUE cCommand_execute_non_query(int argc, VALUE *argv, VALUE self) {
  VALUE query = build_query_from_args(self, argc, argv);
  VALUE connection = rb_iv_get(self, "@connection");
  VALUE sqlite3_connection = rb_iv_get(connection, "@connection");

  if (sqlite3_connection == Qnil) {
    rb_raise(eConnectionError, "This connection has already been closed.");
  }

  sqlite3 *db = NULL;

  Data_Get_Struct(sqlite3_connection, sqlite3, db);

  struct timeval start;
  char *error_message;
  int status;

  gettimeofday(&start, NULL);
  status = sqlite3_exec(db, rb_str_ptr_readonly(query), 0, 0, &error_message);

  if (status != SQLITE_OK) {
    raise_error(self, db, query);
  }

  data_objects_debug(connection, query, &start);

  int affected_rows = sqlite3_changes(db);
  do_int64 insert_id = sqlite3_last_insert_rowid(db);

  return rb_funcall(cResult, ID_NEW, 3, self, INT2NUM(affected_rows), INT2NUM(insert_id));
}

VALUE cCommand_execute_reader(int argc, VALUE *argv, VALUE self) {
  VALUE query = build_query_from_args(self, argc, argv);
  VALUE connection = rb_iv_get(self, "@connection");
  VALUE sqlite3_connection = rb_iv_get(connection, "@connection");

  if (sqlite3_connection == Qnil) {
    rb_raise(eConnectionError, "This connection has already been closed.");
  }

  sqlite3 *db = NULL;

  Data_Get_Struct(sqlite3_connection, sqlite3, db);

  sqlite3_stmt *sqlite3_reader;
  struct timeval start;
  int status;

  gettimeofday(&start, NULL);
  status = sqlite3_prepare_v2(db, rb_str_ptr_readonly(query), -1, &sqlite3_reader, 0);
  data_objects_debug(connection, query, &start);

  if (status != SQLITE_OK) {
    raise_error(self, db, query);
  }

  int field_count = sqlite3_column_count(sqlite3_reader);
  VALUE reader = rb_funcall(cReader, ID_NEW, 0);

  rb_iv_set(reader, "@reader", Data_Wrap_Struct(rb_cObject, 0, 0, sqlite3_reader));
  rb_iv_set(reader, "@field_count", INT2NUM(field_count));
  rb_iv_set(reader, "@connection", connection);

  VALUE field_types = rb_iv_get(self, "@field_types");

  if (field_types == Qnil || RARRAY_LEN(field_types) == 0) {
    field_types = rb_ary_new();
  }
  else if (RARRAY_LEN(field_types) != field_count) {
    // Whoops...  wrong number of types passed to set_types.  Close the reader and raise
    // and error
    rb_funcall(reader, rb_intern("close"), 0);
    rb_raise(rb_eArgError, "Field-count mismatch. Expected %ld fields, but the query yielded %d", RARRAY_LEN(field_types), field_count);
  }

  VALUE field_names = rb_ary_new();
  int i;

  for (i = 0; i < field_count; i++) {
    rb_ary_push(field_names, rb_str_new2((char *)sqlite3_column_name(sqlite3_reader, i)));
  }

  rb_iv_set(reader, "@fields", field_names);
  rb_iv_set(reader, "@field_types", field_types);
  return reader;
}

VALUE cReader_close(VALUE self) {
  VALUE reader_obj = rb_iv_get(self, "@reader");

  if (reader_obj != Qnil) {
    sqlite3_stmt *reader = NULL;

    Data_Get_Struct(reader_obj, sqlite3_stmt, reader);
    sqlite3_finalize(reader);
    rb_iv_set(self, "@reader", Qnil);
    return Qtrue;
  }

  return Qfalse;
}

VALUE cReader_next(VALUE self) {
  if (rb_iv_get(self, "@done") == Qtrue) {
    return Qfalse;
  }

  sqlite3_stmt *reader = NULL;
  int result;

  Data_Get_Struct(rb_iv_get(self, "@reader"), sqlite3_stmt, reader);

  result = sqlite3_step(reader);
  rb_iv_set(self, "@state", INT2NUM(result));

  if (result != SQLITE_ROW) {
    rb_iv_set(self, "@values", Qnil);
    rb_iv_set(self, "@done", Qtrue);
    return Qfalse;
  }

  int enc = -1;
#ifdef HAVE_RUBY_ENCODING_H
  VALUE encoding_id = rb_iv_get(rb_iv_get(self, "@connection"), "@encoding_id");

  if (encoding_id != Qnil) {
    enc = FIX2INT(encoding_id);
  }
#endif

  VALUE field_types = rb_iv_get(self, "@field_types");
  int field_count = NUM2INT(rb_iv_get(self, "@field_count"));
  VALUE arr = rb_ary_new();
  VALUE field_type;
  VALUE value;
  int i;

  for (i = 0; i < field_count; i++) {
    field_type = rb_ary_entry(field_types, i);
    value = typecast(reader, i, field_type, enc);
    rb_ary_push(arr, value);
  }

  rb_iv_set(self, "@values", arr);
  return Qtrue;
}

VALUE cReader_values_sqlite(VALUE self) {
  VALUE state = rb_iv_get(self, "@state");

  if (state == Qnil || NUM2INT(state) != SQLITE_ROW) {
    rb_raise(eDataError, "Reader is not initialized");
    return Qnil;
  }

  return rb_iv_get(self, "@values");
}

void Init_do_sqlite3() {
  common_init();

  mSqlite3 = rb_define_module_under(mDO, "Sqlite3");

  cConnection = rb_define_class_under(mSqlite3, "Connection", cDO_Connection);
  rb_define_method(cConnection, "initialize", cConnection_initialize, 1);
  rb_define_method(cConnection, "dispose", cConnection_dispose, 0);
  rb_define_method(cConnection, "quote_boolean", cConnection_quote_boolean, 1);
  rb_define_method(cConnection, "quote_string", cConnection_quote_string, 1);
  rb_define_method(cConnection, "quote_byte_array", cConnection_quote_byte_array, 1);
  rb_define_method(cConnection, "character_set", cConnection_character_set, 0);
  rb_define_method(cConnection, "enable_load_extension", cConnection_enable_load_extension, 1);
  rb_define_method(cConnection, "load_extension", cConnection_load_extension, 1);

  cCommand = rb_define_class_under(mSqlite3, "Command", cDO_Command);
  rb_define_method(cCommand, "set_types", cCommand_set_types, -1);
  rb_define_method(cCommand, "execute_non_query", cCommand_execute_non_query, -1);
  rb_define_method(cCommand, "execute_reader", cCommand_execute_reader, -1);

  cResult = rb_define_class_under(mSqlite3, "Result", cDO_Result);

  cReader = rb_define_class_under(mSqlite3, "Reader", cDO_Reader);
  rb_define_method(cReader, "close", cReader_close, 0);
  rb_define_method(cReader, "next!", cReader_next, 0);
  rb_define_method(cReader, "values", cReader_values_sqlite, 0); // TODO: DRY?
  rb_define_method(cReader, "fields", cReader_fields, 0);
  rb_define_method(cReader, "field_count", cReader_field_count, 0);

  rb_global_variable(&cResult);
  rb_global_variable(&cReader);

  OPEN_FLAG_READONLY = rb_str_new2("read_only");
  rb_global_variable(&OPEN_FLAG_READONLY);
  OPEN_FLAG_READWRITE = rb_str_new2("read_write");
  rb_global_variable(&OPEN_FLAG_READWRITE);
  OPEN_FLAG_CREATE = rb_str_new2("create");
  rb_global_variable(&OPEN_FLAG_CREATE);
  OPEN_FLAG_NO_MUTEX = rb_str_new2("no_mutex");
  rb_global_variable(&OPEN_FLAG_NO_MUTEX);
  OPEN_FLAG_FULL_MUTEX = rb_str_new2("full_mutex");
  rb_global_variable(&OPEN_FLAG_FULL_MUTEX);

  Init_do_sqlite3_extension();

  do_define_errors(mSqlite3, errors);
}
