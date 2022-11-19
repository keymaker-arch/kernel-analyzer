#include <string>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Value.h>

#include <vector>
#include <regex>

#include "FmtStrParser.h"


using namespace std;

void FmtStrParser::replaceAll(string& str, const string& from, const string& to) {
  if(from.empty())
    return;
  size_t start_pos = 0;
  while((start_pos = str.find(from, start_pos)) != string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
}


// expand conversion specifier to its longest form
unsigned long FmtStrParser::getFinalStrLen(string raw){
  if(raw.empty()) return 0;
  // ignore the %s case
  if(regex_match(raw, regex("%s"))) return 0;

  string final_string = raw;
  replaceAll(final_string, "\n", "NL");
  replaceAll(final_string, "\t", "TB");
  llvm::errs() << "[INFO] format string: " << final_string << "\n";

  final_string = regex_replace(final_string, regex("%(\\.)*[0-9]*[du]"), "aaaaaaaaaa");
  final_string = regex_replace(final_string, regex("%(\\.)*[0-9]*h[du]"), "aaaaa");
  final_string = regex_replace(final_string, regex("%(\\.)*[0-9]*hh[du]"), "aaa");
  final_string = regex_replace(final_string, regex("%(\\.)*[0-9]*ld"), "aaaaaaaaaaaaaaaaaaa");
  final_string = regex_replace(final_string, regex("%(\\.)*[0-9]*lld"), "aaaaaaaaaaaaaaaaaaa");
  final_string = regex_replace(final_string, regex("%(\\.)*[0-9]*lu"), "aaaaaaaaaaaaaaaaaaaa");
  final_string = regex_replace(final_string, regex("%(\\.)*[0-9]*llu"), "aaaaaaaaaaaaaaaaaaaa");
  final_string = regex_replace(final_string, regex("%(\\.)*[0-9]*x"), "aaaaaaaa");
  final_string = regex_replace(final_string, regex("%(\\.)*[0-9]*hx"), "aaaa");
  final_string = regex_replace(final_string, regex("%(\\.)*[0-9]*hhx"), "aa");
  final_string = regex_replace(final_string, regex("%(\\.)*[0-9]*llx"), "aaaaaaaaaaaaaaaa");
  final_string = regex_replace(final_string, regex("%(\\.)*[0-9]*lx"), "aaaaaaaaaaaaaaaa");
  final_string = regex_replace(final_string, regex("%c"), "a");

  return final_string.length();
}