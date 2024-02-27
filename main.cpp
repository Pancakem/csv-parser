#include <cinttypes>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy_ext/report_error.hpp>

struct CSVData {
  std::vector<std::vector<std::string>> rows;

  void print() {
    for (auto row : rows) {
      for (int i = 0; i < row.size() - 1; i++) {
        std::cout << row[i] << ",";
      }
      std::cout << row[row.size() - 1] << "\n";
    }
  }
};

namespace grammar {
namespace dsl = lexy::dsl;

constexpr auto escaped_symbols = lexy::symbol_table<char>
                                         .map<'"'>('"')
                                         .map<'\''>('\'')
                                         .map<'\\'>('\\')
                                         .map<'/'>('/')
                                         .map<'b'>('\b')
                                         .map<'f'>('\f')
                                         .map<'n'>('\n')
                                         .map<'r'>('\r')
                                         .map<'t'>('\t');

constexpr auto escaped_quote = lexy::symbol_table<char>.map<'"'>('"');

struct str_lit {

  static constexpr auto rule = [] {
    auto cp = -dsl::ascii::control;

    auto back_escape = lexy::dsl::backslash_escape.symbol<escaped_symbols>();

    auto quote_escape = lexy::dsl::escape(lexy::dsl::lit_c<'"'>)
                            .template symbol<escaped_quote>();

    return lexy::dsl::delimited(
        lexy::dsl::lit_c<'"'>,
        lexy::dsl::not_followed_by(lexy::dsl::lit_c<'"'>,
                                   lexy::dsl::lit_c<'"'>))(cp, back_escape,
                                                           quote_escape);
  }();

  static constexpr auto value =
      lexy::as_string<std::string, lexy::utf8_encoding>;
};

struct str_val {
  static constexpr auto rule = [] {
    auto escape_character_check =
        dsl::ascii::character - (dsl::ascii::control - dsl::ascii::newline);

    auto id_segment =
        dsl::identifier(escape_character_check - dsl::lit_b<'\\'>);

    auto escape_segment = dsl::token(escape_character_check);
    auto escape_symbol = dsl::symbol<escaped_symbols>(escape_segment);
    auto escape_rule = dsl::lit_b<'\\'> >> escape_symbol;
    return dsl::list(id_segment | escape_rule);
  }();

  static constexpr auto value =
      lexy::as_string<std::string, lexy::utf8_encoding>;
};

struct column {

  static constexpr auto rule = [] {
    return dsl::p<str_lit> | dsl::p<str_val>;
  }();

  static constexpr auto value =
      lexy::as_string<std::string, lexy::utf8_encoding>;
};

struct row {

  static constexpr auto rule = dsl::list(dsl::p<column>, dsl::sep(dsl::comma));

  static constexpr auto value = lexy::as_list<std::vector<std::string>>;
};

struct csv {

  static constexpr auto rule = [] {
    return dsl::terminator(dsl::eof).opt_list(dsl::p<row>);
  }();

  static constexpr auto value =
      lexy::as_list<std::vector<std::vector<std::string>>>;
};

} // namespace grammar

int load_test_cases(std::string dir,
                    std::unique_ptr<std::vector<std::string>> &tests) {
  int rows = 0;
  namespace fs = std::filesystem;
  if (!fs::exists(dir)) {
    return rows;
  }

  for (const auto &entry : fs::directory_iterator(dir)) {
    std::string filename = entry.path().string();
    tests->push_back(filename);
    rows++;
  }

  return rows;
}

int main(int argc, char *argv[]) {

  std::unique_ptr<std::vector<std::string>> tests =
      std::make_unique<std::vector<std::string>>();

  int rows = load_test_cases("./tests", tests);

  for (auto test : *tests) {
    std::cout << "Testing " << test << "\n";
    auto file = lexy::read_file<lexy::utf8_encoding>(test.c_str());
    if (!file) {
      std::cout << "failed to open the file\n";
      continue;
    }

    auto result = lexy::parse<grammar::csv>(
        file.buffer(), lexy_ext::report_error.path(test.c_str()));
    if (!result) {
      std::cout << "failed to parse csv file\n";
    }

    if (result.has_value()) {
      std::cout << "TEST PASSED\n\n";
    } else {
      std::cout << "TEST FAILED\n\n";
    }

    std::cout << "\n\n";
  }

  return 0;
}
