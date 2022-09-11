#include <database/db_structs.hpp>

static const char* g_inner ="INNER JOIN ";
static const char* g_outer = "LEFT OUTER JOIN ";

template <typename T>
std::string filter_statement(T filter);
//  ┌──────────────────────────────────────────┐  //
//  │░░░░░░░░░░░░░░ Helper utils ░░░░░░░░░░░░░░░│  //
//  └──────────────────────────────────────────┘  //

std::string fields_string(std::vector<std::string> fields)
{
  std::string field_string = "";
  std::string delim        = "";

  for (const auto &field : fields)
  {
    field_string += delim + field;
    delim         = ",";
  }

  return field_string;
}
//**************************************************//
std::string values_string(StringVec values, size_t number_of_fields)
{
  std::string value_string{"VALUES ("};
  std::string delim{};
  int         index{1};

  for (const auto &value : values)
  {
    delim = (index++ % number_of_fields == 0) ? "),(" : ",";
    value_string += "'";
    value_string += (value.empty()) ? "NULL" : DoubleSingleQuotes(value);
    value_string += "'" + delim;
  }
  value_string.erase(value_string.end() - 2, value_string.end());

  return value_string;
}
//**************************************************//
static std::string order_string(const OrderFilter& filter)
{
  return " ORDER BY " + filter.field + ' ' + filter.order;
}
//**************************************************//
static std::string limit_string(const std::string& number)
{
  return " LIMIT " + number;
}
//**************************************************//
std::string get_join_string(Joins joins)
{
  std::string join_s{};
  for (const auto& join : joins)
  {
    join_s += join.type == JoinType::INNER ? g_inner : g_outer;
    join_s += join.table + " ON " + join.table + '.' + join.field + '=' + join.join_table + '.' + join.join_field;
    join_s += " ";
  }

  if (join_s.size())
    join_s.pop_back(); // remove whitespace

  return join_s;
}
//**************************************************//
std::string insert_statement(DatabaseQuery query)
{
  return "INSERT INTO " + query.table + "("   +
          fields_string(query.fields) + ") " +
          values_string(query.values, query.fields.size());
}
//**************************************************//
std::string insert_statement(InsertReturnQuery query, std::string returning)
{
  if (returning.empty())
    return "INSERT INTO " + query.table  + "("  +
            fields_string(query.fields) + ") " +
            values_string(query.values, query.fields.size());
  else
    return "INSERT INTO " + query.table  + "("                +
            fields_string(query.fields) + ") "               +
            values_string(query.values, query.fields.size()) +
            " RETURNING " + returning;
}

//**************************************************//
// To filter properly, you must have the same number of values as fields
std::string update_statement(UpdateReturnQuery query, std::string returning,
                            bool multiple = false)
{
  const auto filter = query.filter.value();
  if (!filter.empty())
  { // TODO: Handle case for updating multiple rows at once
    if (!multiple)
    {
      std::string filter_string{"WHERE "};
      std::string update_string{"SET "};
      std::string delim = "";
      filter_string += filter_statement(filter);
      if (query.values.size() == query.fields.size()) // can only update if the `fields` and
      {                                               // `values` arguments are matching
        for (uint8_t i = 0; i < query.values.size(); i++)
        {
          const auto field = query.fields.at(i);
          const auto value = query.values.at(i);
          update_string += delim + field + "=" + "'" + value + "'";
          delim = ',';
        }
      }
      return "UPDATE " + query.table + " " + update_string + " " + filter_string + " RETURNING " + returning;
    }
  }
  return "";
}
//**************************************************//
template <typename T>
std::string delete_statement(T query)
{
  std::string stmt;
  const auto filter = query.filter.value();
  if (filter.empty()) stmt = "";
  else
  if constexpr (std::is_same_v<T, DatabaseQuery>)
    stmt =
      "DELETE FROM " + query.table          + " " +
      "WHERE "       + filter.front().first + "='" + filter.front().second + "'" +
     " RETURNING "   + filter.front().first;
  return stmt;
}

//  ┌─────────────────────────────────────┐  //
//  │░░░░░░░░░░░░░░ Visitors ░░░░░░░░░░░░░░│  //
//  └─────────────────────────────────────┘  //
template <typename T>
struct FilterVisitor
{
FilterVisitor(T filters)
{
  operator()(filters);
}

std::string
value() const
{
  return _M_out;
}
//**************************************************//
void
operator()(const MultiOptionFilter& f)
{
  _M_out += f.a + " " + f.comparison + " (";
  std::string delim{};
  for (const auto &option : f.options)
  {
    _M_out += delim + option;
    delim   = ",";
  }
  _M_out += ")";
}
//**************************************************//
void
operator()(const CompBetweenFilter& filter)
{
  _M_out += filter.field + " BETWEEN " + filter.a + " AND " + filter.b;
}
//**************************************************//
void
operator()(const CompFilter& filter)
{
  _M_out += filter.a + filter.sign + filter.b;
}
//**************************************************//
void
operator()(const QueryComparisonFilter& filter)
{
  _M_out += std::get<0>(filter[0]) + std::get<1>(filter[0]) + std::get<2>(filter[0]);
}
//**************************************************//
void
operator()(const QueryFilter& filter)
{
  std::string delim{};
  for (const auto& f : filter)
  {
    _M_out += delim + f.first + '=' + '\'' + f.second + '\'';
    delim   = " AND ";
  }
}
//**************************************************//
void
operator()(QueryFilter::Filters filters)
{
  std::string delim{};
  for (const auto& f : filters)
  {
    _M_out += delim + f.first + '=' + '\'' + f.second + '\'';
    delim   = " AND ";
  }
}
//**************************************************//
void
operator()(GenericFilter filter)
{
  _M_out = filter.a + filter.comparison + filter.b;
}

std::string _M_out;
};

//  ┌─────────────────────────────────────┐  //
//  │░░░░░░░░░░ Visitor Helpers ░░░░░░░░░░░│  //
//  └─────────────────────────────────────┘  //
template <typename T>
std::string filter_statement(T filter)
{
  return FilterVisitor{filter}.value();
}
//**************************************************//
template <typename FilterA, typename FilterB>
std::string variant_filter_statement(std::vector<std::variant<FilterA, FilterB>> filters)
{
  std::string filter_string{};
  uint8_t     idx          = 0;
  uint8_t     filter_count = filters.size();

  for (const auto &filter : filters)
  {
    filter_string += (filter.index() == 0) ? filter_statement(std::get<0>(filter)) :
                                             filter_statement(std::get<1>(filter));
    if (idx++ < filter_count - 1)
      filter_string += " AND ";
  }

  return filter_string;
}
//**************************************************//
template <typename FilterA, typename FilterB, typename FilterC>
std::string variant_filter_statement(std::vector<std::variant<FilterA, FilterB, FilterC>> filters)
{
  std::string filter_string{};
  uint8_t     idx          = 0;
  uint8_t     filter_count = filters.size();

  for (const auto &filter : filters)
  {
    switch (filter.index())
    {
    case (0): filter_string += filter_statement(std::get<0>(filter));
    break;
    case (1): filter_string += filter_statement(std::get<1>(filter));
      break;
    default:  filter_string += filter_statement(std::get<2>(filter));
    }

    if (idx++ < filter_count - 1)
      filter_string += " AND ";
  }

  return filter_string;
}

//******************************************************************************************//

static const char* UNSUPPORTED = "SELECT 1";
template <typename T>
struct SelectVisitor
{
std::string delim         = "";
std::string filter_string = " WHERE ";
std::string _M_out;

SelectVisitor(T query)
{

  if (query.filter.size())
    operator()(query);
  else
  {
    if constexpr(std::is_same_v<T, JoinQuery<QueryFilter>>)
      _M_out = "SELECT " + fields_string(query.fields) + " FROM " + query.table + ' ' + get_join_string(query.joins);
    else
    if constexpr(std::is_same_v<T, SimpleJoinQuery>)
      _M_out = "SELECT " + fields_string(query.fields) + " FROM " + query.table + ' ' + get_join_string({query.join});
    else
      _M_out = "SELECT " + fields_string(query.fields) + " FROM " + query.table;
  }
}
//**************************************************//
std::string
value() const
{
  return _M_out;
}
//**************************************************//
void
operator()(DatabaseQuery query)
{
  const auto filter = query.filter;
  if (filter.size() > 1 &&
      filter.front().first == filter.at(1).first)
  {
    filter_string += filter.front().first + " in (";
    for (const auto &filter_pair : filter)
    {
      filter_string += delim + filter_pair.second;
      delim = ",";
    }
    _M_out = "SELECT " + fields_string(query.fields) + " FROM " + query.table + filter_string + ")";
  }
  else
  {
    for (const auto &filter_pair : filter)
    {
      filter_string += delim + filter_pair.first + "='" + filter_pair.second + "'";
      delim = " AND ";
    }
    _M_out = "SELECT " + fields_string(query.fields) + " FROM " + query.table + filter_string;
  }
}
//**************************************************//
void
operator()(ComparisonSelectQuery query)
{
  if (query.filter.size() > 1)
  {
    _M_out = UNSUPPORTED;
    return;
  }

  for (const auto &filter_tup : query.filter)
  {
    filter_string += delim + std::get<0>(filter_tup) + std::get<1>(filter_tup) + std::get<2>(filter_tup);
    delim = " AND ";
  }
  _M_out = "SELECT " + fields_string(query.fields) + " FROM " + query.table + filter_string;
}
//**************************************************//
void
operator()(ComparisonBetweenSelectQuery query)
{
  if (query.filter.size() > 1)
  {
    _M_out = UNSUPPORTED;
    return;
  }

  for (const auto &filter : query.filter)
    filter_string += delim + filter_statement(filter);
  _M_out = "SELECT " + fields_string(query.fields) + " FROM " + query.table + filter_string;
}
//**************************************************//
void
operator()(MultiFilterSelect query)
{
  for (const auto &filter : query.filter)
  {
    filter_string += delim + filter_statement(filter);
    delim = " AND ";
  }
  _M_out = "SELECT " + fields_string(query.fields) + " FROM " + query.table + filter_string;
}
//**************************************************//
void
operator()(MultiVariantFilterSelect<std::vector<std::variant<CompFilter, CompBetweenFilter>>> query)
{
  std::string stmt{"SELECT " + fields_string(query.fields) + " FROM " + query.table + filter_string +
    variant_filter_statement<CompFilter, CompBetweenFilter>(query.filter)};
  if (query.order.has_value()) stmt += order_string(query.order);
  if (query.limit.has_value()) stmt += limit_string(query.limit.count);
  _M_out = stmt;
}
//**************************************************//
void
operator()(MultiVariantFilterSelect<std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>> query)
{
  std::string stmt{"SELECT " + fields_string(query.fields) + " FROM " + query.table + filter_string +
    variant_filter_statement<CompFilter, CompBetweenFilter, MultiOptionFilter>(query.filter)};
  if (query.order.has_value()) stmt += order_string(query.order);
  if (query.limit.has_value()) stmt += limit_string(query.limit.count);
  _M_out = stmt;
}
//**************************************************//
void
operator()(MultiVariantFilterSelect<std::vector<std::variant<CompBetweenFilter, QueryFilter>>> query)
{
  std::string stmt{"SELECT " + fields_string(query.fields) + " FROM " + query.table + filter_string +
                    variant_filter_statement<CompBetweenFilter, QueryFilter>(query.filter)};
  if (query.order.has_value()) stmt += order_string(query.order);
  if (query.limit.has_value()) stmt += limit_string(query.limit.count);
  _M_out = stmt;
}
//**************************************************//
void
operator()(MultiVariantFilterSelect<std::vector<std::variant<QueryComparisonFilter, QueryFilter>>> query)
{
  std::string stmt{"SELECT " + fields_string(query.fields) + " FROM " + query.table + filter_string +
                    variant_filter_statement<QueryComparisonFilter, QueryFilter>(query.filter)};
  if (query.order.has_value()) stmt += order_string(query.order);
  if (query.limit.has_value()) stmt += limit_string(query.limit.count);
  _M_out = stmt;
}
//**************************************************//
void
operator()(JoinQuery<std::vector<std::variant<CompFilter, CompBetweenFilter, MultiOptionFilter>>> query)
{
  filter_string += variant_filter_statement(query.filter);
  _M_out = "SELECT " + fields_string(query.fields) + " FROM " + query.table + " " + get_join_string(query.joins) + filter_string;
}
//**************************************************//
void
operator()(SimpleJoinQuery query)
{
  filter_string += filter_statement(query.filter);
  _M_out = "SELECT " + fields_string(query.fields) + " FROM " + query.table + " " + get_join_string({query.join}) + filter_string;
}
//**************************************************//
void
operator()(JoinQuery<std::vector<QueryFilter>> query)
{
  for (const auto &f : query.filter)
  {
    filter_string += delim + filter_statement(f);
    delim = " AND ";
  }
  std::string join_string = get_join_string(query.joins);
  _M_out = "SELECT " + fields_string(query.fields) + " FROM " + query.table + " " + join_string + filter_string;
}
//**************************************************//
void
operator()(JoinQuery<QueryFilter> query)
{
  filter_string          += delim + filter_statement(query.filter);
  std::string join_string = get_join_string(query.joins);
  _M_out = "SELECT " + fields_string(query.fields) + " FROM " + query.table + " " + join_string + filter_string;
}
};

//**************************************************//
template <typename T>
std::string select_statement(T query)
{
  return SelectVisitor{query}.value();
}
