/* Copyright (c) 2006, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_UPDATE_INCLUDED
#define SQL_UPDATE_INCLUDED

#include <stddef.h>
#include <sys/types.h>

#include "my_base.h"
#include "my_global.h"
#include "my_sqlcommand.h"
#include "probes_mysql.h"    // IWYU pragma: keep
#include "query_result.h"    // Query_result_interceptor
#include "sql_cmd_dml.h"     // Sql_cmd_dml
#include "sql_lex.h"
#include "sql_list.h"

class COPY_INFO;
class Copy_field;
class Item;
class JOIN;
class THD;
class Temp_table_param;
struct TABLE;
struct TABLE_LIST;

bool records_are_comparable(const TABLE *table);
bool compare_records(const TABLE *table);

class Query_result_update final : public Query_result_interceptor
{
  /// Number of tables being updated
  uint update_table_count;
  /// Pointer to list of updated tables, linked via 'next_local'
  TABLE_LIST *update_tables;
  /// Array of references to temporary tables used to store cached updates
  TABLE **tmp_tables;
  /// Array of parameter structs for creation of temporary tables
  Temp_table_param *tmp_table_param;
  /// The first table in the join operation
  TABLE *main_table;
  /// ???
  TABLE *table_to_update;
  /// Number of rows found that matches join and WHERE conditions
  ha_rows found_rows;
  /// Number of rows actually updated, in all affected tables
  ha_rows updated_rows;
  /// List of pointers to fields to update, in order from statement
  List <Item> *fields;
  /// List of pointers to values to update with, in order from statement
  List <Item> *values;
  /// The fields list decomposed into separate lists per table
  List <Item> **fields_for_table;
  /// The values list decomposed into separate lists per table
  List <Item> **values_for_table;
  /**
   List of tables referenced in the CHECK OPTION condition of
   the updated view excluding the updated table. 
  */
  List <TABLE> unupdated_check_opt_tables;
  /// ???
  Copy_field *copy_field;
  /// Whether to perform updates or not, cleared when updates are done.
  bool do_update;
  /// True if all tables to be updated are transactional.
  bool trans_safe;
  /// True if the update operation has made a change in a transactional table
  bool transactional_tables;
  /**
     error handling (rollback and binlogging) can happen in send_eof()
     so that afterward send_error() needs to find out that.
  */
  bool error_handled;

  /**
     Array of update operations, arranged per _updated_ table. For each
     _updated_ table in the multiple table update statement, a COPY_INFO
     pointer is present at the table's position in this array.

     The array is allocated and populated during Query_result_update::prepare().
     The position that each table is assigned is also given here and is stored
     in the member TABLE::pos_in_table_list::shared. However, this is a publicly
     available field, so nothing can be trusted about its integrity.

     This member is NULL when the Query_result_update is created.

     @see Query_result_update::prepare
  */
  COPY_INFO **update_operations;

public:
  Query_result_update(THD *thd, List<Item> *field_list, List<Item> *value_list)
  :Query_result_interceptor(thd),
   update_table_count(0),
   update_tables(NULL), tmp_tables(NULL),
   main_table(NULL), table_to_update(NULL),
   found_rows(0), updated_rows(0),
   fields(field_list), values(value_list),
   copy_field(NULL),
   do_update(true), trans_safe(true),
   transactional_tables(false), error_handled(false),
   update_operations(NULL)
  {}
  ~Query_result_update()
  {}
  bool need_explain_interceptor() const override { return true; }
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u) override;
  bool send_data(List<Item> &items) override;
  bool initialize_tables (JOIN *join) override;
  void send_error(uint errcode, const char *err) override;
  bool do_updates();
  bool send_eof() override;
  void abort_result_set() override;
  void cleanup() override;
};

class Sql_cmd_update final : public Sql_cmd_dml
{
public:
  Sql_cmd_update(bool multitable_arg, List<Item> *update_values)
  : multitable(multitable_arg), update_value_list(update_values) {}

  enum_sql_command sql_command_code() const override
  { return lex->sql_command; }

  bool is_single_table_plan() const override { return !multitable; }

protected:
  bool precheck(THD *thd) override;

  bool prepare_inner(THD *thd) override;

  bool execute_inner(THD *thd) override;

#if defined(HAVE_DTRACE) && !defined(DISABLE_DTRACE)
  void start_stmt_dtrace(char *query) override
  {
    if (multitable)
      MYSQL_MULTI_UPDATE_START(query);
    else
      MYSQL_UPDATE_START(query);
  }
  void end_stmt_dtrace(int status, ulonglong rows, ulonglong changed) override
  {
    if (multitable)
      MYSQL_UPDATE_DONE(status, rows, changed);
    else
      MYSQL_MULTI_UPDATE_DONE(status, rows, changed);
  }
#endif

private:
  bool update_single_table(THD *thd);

  bool multitable;
  List<Item> *update_value_list;
};

#endif /* SQL_UPDATE_INCLUDED */
