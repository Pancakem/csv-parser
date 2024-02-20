#include <cinttypes>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
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
    auto lead_char = dsl::ascii::alpha / dsl::ascii::alpha_digit;
    auto trailing_char = dsl::ascii::word / dsl::ascii::punct;
    return dsl::identifier(lead_char, trailing_char);
  }();

  static constexpr auto value = lexy::as_string<std::string>;
};

struct column {

  static constexpr auto rule = [] {
    auto cp = -dsl::ascii::control;

    auto back_escape = lexy::dsl::backslash_escape.symbol<escaped_symbols>();

    auto quote_escape = lexy::dsl::escape(lexy::dsl::lit_c<'"'>)
                            .template symbol<escaped_quote>();

    return lexy::dsl::delimited(
               lexy::dsl::lit_c<'"'>,
               lexy::dsl::not_followed_by(lexy::dsl::lit_c<'"'>,
                                          lexy::dsl::lit_c<'"'>))(
               cp, back_escape, quote_escape) |
           dsl::else_ >> dsl::p<str_lit>;
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
    // auto if_empty = dsl::if_(dsl::eof >> dsl::return_);
    // return if_empty + dsl::recurse<row>;
    return dsl::terminator(dsl::eof).opt_list(dsl::p<row>);
  }();

  static constexpr auto value =
      lexy::as_list<std::vector<std::vector<std::string>>>;
};

} // namespace grammar

struct Test {
  std::string filename;
  int result;
};

int get_line_count(std::string path) {
  int count = 0;
  std::ifstream infile(path);

  if (infile.is_open()) {
    std::string line;

    while (std::getline(infile, line)) {
      count++;
    }

    infile.close();
  }

  return count;
}

bool load_test_cases(std::string dir, std::map<const char *, Test> &tests) {
  namespace fs = std::filesystem;
  if (!fs::exists(dir)) {
    return false;
  }

  for (const auto &entry : fs::directory_iterator(dir)) {
    std::string filename = entry.path().string();
    Test case_ = {filename, get_line_count(filename)};
    tests.insert({filename.c_str(), case_});
  }

  return true;
}

int main(int argc, char *argv[]) {

  std::map<const char *, Test> tests = std::map<const char *, Test>();
  load_test_cases("./tests", tests);

  for (auto test : tests) {
    std::fprintf(stdout, "Testing %s\n", test.first);
    auto file =
        lexy::read_file<lexy::utf8_encoding>(test.second.filename.c_str());
    if (!file) {
      std::fprintf(stderr, "failed to open the file\n");
      return -1;
    }

    auto result = lexy::parse<grammar::csv>(
        file.buffer(),
        lexy_ext::report_error.path(test.second.filename.c_str()));
    if (!result) {
      std::fprintf(stderr, "failed to parse csv file\n");
      return -1;
    }

    if (result.has_value()) {
      CSVData data = CSVData();
      data.rows = result.value();
      std::fprintf(stdout, "parsed %" PRIu64 " row(s)\n", data.rows.size());
      data.print();

      if (data.rows.size() == test.second.result) {
        std::fprintf(stdout, "TEST PASSED");
      } else {
        std::fprintf(stdout, "TEST FAILED");
      }
    }

    std::fprintf(stdout, "\n\n");
  }

  return 0;
}
