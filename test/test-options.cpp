// Copyright (C) 2020  Matthew "strager" Glazar
// See end of file for extended copyright information.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <initializer_list>
#include <iostream>
#include <quick-lint-js/cli/options.h>
#include <quick-lint-js/diag/diag-code-list.h>
#include <quick-lint-js/diag/diagnostic-types.h>
#include <quick-lint-js/io/output-stream.h>
#include <quick-lint-js/util/narrow-cast.h>
#include <string_view>
#include <vector>

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using namespace std::literals::string_view_literals;

namespace quick_lint_js {
namespace {
Options parse_options(std::initializer_list<const char *> arguments) {
  std::vector<char *> argv;
  argv.emplace_back(const_cast<char *>("(program)"));
  for (const char *argument : arguments) {
    argv.emplace_back(const_cast<char *>(argument));
  }
  return quick_lint_js::parse_options(narrow_cast<int>(argv.size()),
                                      argv.data());
}

Options parse_options_no_errors(std::initializer_list<const char *> arguments) {
  Options o = parse_options(arguments);
  EXPECT_THAT(o.error_unrecognized_options, IsEmpty());
  EXPECT_THAT(o.warning_language_without_file, IsEmpty());
  EXPECT_THAT(o.warning_vim_bufnr_without_file, IsEmpty());
  return o;
}

struct Dumped_Errors {
  bool have_errors;
  String8 output;
};

Dumped_Errors dump_errors(const Options &o) {
  Memory_Output_Stream output;
  bool have_errors = o.dump_errors(output);
  output.flush();
  return Dumped_Errors{
      .have_errors = have_errors,
      .output = output.get_flushed_string8(),
  };
}

TEST(Test_Options, default_options_with_no_files) {
  Options o = parse_options_no_errors({});
  EXPECT_FALSE(o.print_parser_visits);
  EXPECT_FALSE(o.help);
  EXPECT_FALSE(o.list_debug_apps);
  EXPECT_FALSE(o.version);
  EXPECT_FALSE(o.lsp_server);
  EXPECT_EQ(o.output_format, Output_Format::default_format);
  EXPECT_THAT(o.files_to_lint, IsEmpty());
}

TEST(Test_Options, default_options_with_files) {
  Options o = parse_options_no_errors({"foo.js"});
  EXPECT_FALSE(o.print_parser_visits);
  EXPECT_FALSE(o.snarky);
  ASSERT_EQ(o.files_to_lint.size(), 1);
  EXPECT_EQ(o.files_to_lint[0].path, "foo.js"sv);
}

TEST(Test_Options, hyphen_hyphen_treats_remaining_arguments_as_files) {
  {
    Options o = parse_options_no_errors({"--", "foo.js"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_EQ(o.files_to_lint[0].path, "foo.js"sv);
  }

  {
    Options o = parse_options_no_errors(
        {"--", "--debug-parser-visits", "foo.js", "-bar"});
    EXPECT_FALSE(o.print_parser_visits);
    ASSERT_EQ(o.files_to_lint.size(), 3);
    EXPECT_EQ(o.files_to_lint[0].path, "--debug-parser-visits"sv);
    EXPECT_EQ(o.files_to_lint[1].path, "foo.js"sv);
    EXPECT_EQ(o.files_to_lint[2].path, "-bar"sv);
  }
}

TEST(Test_Options, debug_parser_visits) {
  Options o = parse_options_no_errors({"--debug-parser-visits", "foo.js"});
  EXPECT_TRUE(o.print_parser_visits);
  ASSERT_EQ(o.files_to_lint.size(), 1);
  EXPECT_EQ(o.files_to_lint[0].path, "foo.js"sv);
}

TEST(Test_Options, snarky) {
  Options o = parse_options_no_errors({"--snarky", "foo.js"});
  EXPECT_TRUE(o.snarky);
  ASSERT_EQ(o.files_to_lint.size(), 1);
  EXPECT_EQ(o.files_to_lint[0].path, "foo.js"sv);
}

TEST(Test_Options, debug_parser_visits_shorthand) {
  {
    Options o = parse_options_no_errors({"--debug-p", "foo.js"});
    EXPECT_TRUE(o.print_parser_visits);
  }

  {
    Options o = parse_options_no_errors({"--debug-parser-vis", "foo.js"});
    EXPECT_TRUE(o.print_parser_visits);
  }
}

TEST(Test_Options, output_format) {
  {
    Options o = parse_options_no_errors({});
    EXPECT_EQ(o.output_format, Output_Format::default_format);
  }

  {
    Options o = parse_options_no_errors({"--output-format=gnu-like"});
    EXPECT_EQ(o.output_format, Output_Format::gnu_like);
  }

  {
    Options o = parse_options_no_errors({"--output-format=vim-qflist-json"});
    EXPECT_EQ(o.output_format, Output_Format::vim_qflist_json);
  }

  {
    Options o = parse_options_no_errors({"--output-format=emacs-lisp"});
    EXPECT_EQ(o.output_format, Output_Format::emacs_lisp);
  }
}

TEST(Test_Options, invalid_output_format) {
  {
    Options o = parse_options({"--output-format=unknown-garbage"});
    EXPECT_THAT(o.error_unrecognized_options,
                ElementsAreArray({"unknown-garbage"sv}));
    EXPECT_EQ(o.output_format, Output_Format::default_format)
        << "output_format should remain the default";
  }

  {
    Options o = parse_options({"--output-format"});
    EXPECT_THAT(o.error_unrecognized_options,
                ElementsAreArray({"--output-format"sv}));
  }
}

TEST(Test_Options, vim_file_bufnr) {
  {
    Options o = parse_options_no_errors({"one.js", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].vim_bufnr, std::nullopt);
    EXPECT_EQ(o.files_to_lint[1].vim_bufnr, std::nullopt);
  }

  {
    Options o = parse_options_no_errors({"--output-format", "vim-qflist-json",
                                         "--vim-file-bufnr", "3", "file.js"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_EQ(o.files_to_lint[0].path, "file.js"sv);
    EXPECT_EQ(o.files_to_lint[0].vim_bufnr, 3);
  }

  {
    Options o =
        parse_options_no_errors({"--vim-file-bufnr", "3", "one.js", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].vim_bufnr, 3);
    EXPECT_EQ(o.files_to_lint[1].vim_bufnr, std::nullopt);
  }

  {
    Options o =
        parse_options_no_errors({"one.js", "--vim-file-bufnr=10", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].vim_bufnr, std::nullopt);
    EXPECT_EQ(o.files_to_lint[1].vim_bufnr, 10);
  }

  {
    Options o = parse_options_no_errors(
        {"--vim-file-bufnr=1", "one.js", "--vim-file-bufnr=2", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].vim_bufnr, 1);
    EXPECT_EQ(o.files_to_lint[1].vim_bufnr, 2);
  }

  {
    Options o = parse_options_no_errors({"--vim-file-bufnr=42", "-"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_EQ(o.files_to_lint[0].vim_bufnr, 42);
  }

  {
    Options o =
        parse_options_no_errors({"one.js", "--vim-file-bufnr=42", "--stdin"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[1].vim_bufnr, 42);
  }

  {
    Options o = parse_options_no_errors(
        {"--vim-file-bufnr=1", "--", "one.js", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].vim_bufnr, 1);
    EXPECT_EQ(o.files_to_lint[1].vim_bufnr, std::nullopt);
  }
}

TEST(Test_Options, path_for_config_search) {
  {
    Options o = parse_options_no_errors({"one.js", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].path_for_config_search, nullptr);
    EXPECT_EQ(o.files_to_lint[1].path_for_config_search, nullptr);
  }

  {
    Options o = parse_options_no_errors(
        {"--path-for-config-search", "configme.js", "file.js"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_EQ(o.files_to_lint[0].path, "file.js"sv);
    EXPECT_STREQ(o.files_to_lint[0].path_for_config_search, "configme.js");
  }

  {
    Options o = parse_options_no_errors(
        {"--path-for-config-search", "configme.js", "one.js", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_STREQ(o.files_to_lint[0].path_for_config_search, "configme.js");
    EXPECT_EQ(o.files_to_lint[1].path_for_config_search, nullptr);
  }

  {
    Options o = parse_options_no_errors(
        {"one.js", "--path-for-config-search=configme.js", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].path_for_config_search, nullptr);
    EXPECT_STREQ(o.files_to_lint[1].path_for_config_search, "configme.js");
  }

  {
    Options o = parse_options_no_errors(
        {"--path-for-config-search=test/one.js", "/tmp/one.js",
         "--path-for-config-search=src/two.js", "/tmp/two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_STREQ(o.files_to_lint[0].path_for_config_search, "test/one.js");
    EXPECT_STREQ(o.files_to_lint[1].path_for_config_search, "src/two.js");
  }

  {
    Options o =
        parse_options_no_errors({"--path-for-config-search=configme.js", "-"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_STREQ(o.files_to_lint[0].path_for_config_search, "configme.js");
  }

  {
    Options o = parse_options_no_errors(
        {"one.js", "--path-for-config-search=configme.js", "--stdin"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_STREQ(o.files_to_lint[1].path_for_config_search, "configme.js");
  }

  {
    Options o = parse_options_no_errors(
        {"--path-for-config-search=configme.js", "--", "one.js", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_STREQ(o.files_to_lint[0].path_for_config_search, "configme.js");
    EXPECT_EQ(o.files_to_lint[1].path_for_config_search, nullptr);
  }

  {
    Options o = parse_options_no_errors(
        {"--path-for-config-search=configme.js", "--stdin", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_STREQ(o.files_to_lint[0].path_for_config_search, "configme.js");
    EXPECT_EQ(o.files_to_lint[1].path_for_config_search, nullptr);
  }
}

TEST(Test_Options, config_file) {
  {
    Options o = parse_options_no_errors({"one.js", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].config_file, nullptr);
    EXPECT_EQ(o.files_to_lint[1].config_file, nullptr);
    EXPECT_FALSE(o.has_config_file);
  }

  {
    Options o =
        parse_options_no_errors({"--config-file", "config.json", "file.js"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_EQ(o.files_to_lint[0].path, "file.js"sv);
    EXPECT_EQ(o.files_to_lint[0].config_file, "config.json"sv);
    EXPECT_TRUE(o.has_config_file);
  }

  {
    Options o = parse_options_no_errors(
        {"--config-file", "config.json", "one.js", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].config_file, "config.json"sv);
    EXPECT_EQ(o.files_to_lint[1].config_file, "config.json"sv);
  }

  {
    Options o = parse_options_no_errors(
        {"one.js", "--config-file=config.json", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].config_file, nullptr);
    EXPECT_EQ(o.files_to_lint[1].config_file, "config.json"sv);
  }

  {
    Options o = parse_options_no_errors({"--config-file=one.config", "one.js",
                                         "--config-file=two.config", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].config_file, "one.config"sv);
    EXPECT_EQ(o.files_to_lint[1].config_file, "two.config"sv);
  }

  {
    Options o = parse_options_no_errors({"--config-file=config.json", "-"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_EQ(o.files_to_lint[0].config_file, "config.json"sv);
  }

  {
    Options o = parse_options_no_errors(
        {"one.js", "--config-file=config.json", "--stdin"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[1].config_file, "config.json"sv);
  }

  {
    Options o = parse_options_no_errors(
        {"--config-file=config.json", "--", "one.js", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].config_file, "config.json"sv);
    EXPECT_EQ(o.files_to_lint[1].config_file, "config.json"sv);
  }
}

TEST(Test_Options, language) {
  {
    Options o =
        parse_options_no_errors({"one.js", "two.ts", "three.txt", "--stdin"});
    ASSERT_EQ(o.files_to_lint.size(), 4);
    EXPECT_EQ(o.files_to_lint[0].language, std::nullopt) << "one.js";
    EXPECT_EQ(o.files_to_lint[1].language, std::nullopt) << "two.ts";
    EXPECT_EQ(o.files_to_lint[2].language, std::nullopt) << "three.txt";
    EXPECT_EQ(o.files_to_lint[3].language, std::nullopt) << "--stdin";
  }

  {
    Options o = parse_options_no_errors(
        {"--language=javascript", "one.js", "two.ts", "three.txt"});
    ASSERT_EQ(o.files_to_lint.size(), 3);
    EXPECT_EQ(o.files_to_lint[0].language, Input_File_Language::javascript);
    EXPECT_EQ(o.files_to_lint[1].language, Input_File_Language::javascript);
    EXPECT_EQ(o.files_to_lint[2].language, Input_File_Language::javascript);
  }

  {
    Options o =
        parse_options_no_errors({"--language=javascript", "one.js",
                                 "--language=javascript-jsx", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].language, Input_File_Language::javascript);
    EXPECT_EQ(o.files_to_lint[1].language, Input_File_Language::javascript_jsx);
  }

  {
    Options o = parse_options_no_errors(
        {"one.js", "--language=javascript-jsx", "two.jsx"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_EQ(o.files_to_lint[0].language, std::nullopt);
    EXPECT_EQ(o.files_to_lint[1].language, Input_File_Language::javascript_jsx);
  }

  {
    Options o = parse_options_no_errors(
        {"--language=experimental-typescript", "one.txt"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_EQ(o.files_to_lint[0].language, Input_File_Language::typescript);
  }

  {
    Options o = parse_options_no_errors(
        {"--language=experimental-typescript-definition", "one.txt"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_EQ(o.files_to_lint[0].language,
              Input_File_Language::typescript_definition);
  }

  {
    Options o = parse_options_no_errors(
        {"--language=experimental-typescript-jsx", "one.txt"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_EQ(o.files_to_lint[0].language, Input_File_Language::typescript_jsx);
  }

  {
    Options o = parse_options_no_errors({"--language=javascript-jsx", "-"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_EQ(o.files_to_lint[0].language, Input_File_Language::javascript_jsx);
  }

  {
    Options o =
        parse_options_no_errors({"--language=javascript-jsx", "--stdin"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_EQ(o.files_to_lint[0].language, Input_File_Language::javascript_jsx);
  }

  {
    Options o = parse_options({"file.js", "--language=javascript-jsx"});
    EXPECT_THAT(o.warning_language_without_file,
                ElementsAreArray({"javascript-jsx"sv}));

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(
        errors.output,
        u8"warning: flag '--language=javascript-jsx' should be followed by an "
        u8"input file name or --stdin\n");
  }

  {
    Options o = parse_options(
        {"--language=javascript", "--language=javascript-jsx", "test.jsx"});
    EXPECT_THAT(o.warning_language_without_file,
                ElementsAreArray({"javascript"sv}));

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(
        errors.output,
        u8"warning: flag '--language=javascript' should be followed by an "
        u8"input file name or --stdin\n");
  }

  {
    Options o = parse_options({"--language=badlanguageid", "test.js"});
    EXPECT_THAT(o.warning_language_without_file, IsEmpty());
    // TODO(strager): Highlight the full option, not just the value.
    EXPECT_THAT(o.error_unrecognized_options,
                ElementsAreArray({"badlanguageid"sv}));
  }
}

TEST(Test_Options, get_language_from_path) {
  constexpr auto javascript_jsx = Input_File_Language::javascript_jsx;
  EXPECT_EQ(get_language("<stdin>", std::nullopt), javascript_jsx);
  EXPECT_EQ(get_language("hi.js", std::nullopt), javascript_jsx);
  EXPECT_EQ(get_language("hi.jsx", std::nullopt), javascript_jsx);
  EXPECT_EQ(get_language("hi.txt", std::nullopt), javascript_jsx);
}

TEST(Test_Options, get_language_overwritten) {
  constexpr auto javascript = Input_File_Language::javascript;
  constexpr auto javascript_jsx = Input_File_Language::javascript_jsx;

  EXPECT_EQ(get_language("<stdin>", javascript_jsx), javascript_jsx);
  EXPECT_EQ(get_language("hi.js", javascript_jsx), javascript_jsx);
  EXPECT_EQ(get_language("hi.jsx", javascript_jsx), javascript_jsx);
  EXPECT_EQ(get_language("hi.txt", javascript_jsx), javascript_jsx);

  EXPECT_EQ(get_language("<stdin>", javascript), javascript);
  EXPECT_EQ(get_language("hi.js", javascript), javascript);
  EXPECT_EQ(get_language("hi.jsx", javascript), javascript);
  EXPECT_EQ(get_language("hi.txt", javascript), javascript);
}

TEST(Test_Options, lsp_server) {
  {
    Options o = parse_options_no_errors({"--lsp-server"});
    EXPECT_TRUE(o.lsp_server);
  }

  {
    Options o = parse_options_no_errors({"--lsp"});
    EXPECT_TRUE(o.lsp_server);
  }
}

TEST(Test_Options, dash_dash_stdin) {
  {
    Options o = parse_options_no_errors({"--stdin", "one.js"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_TRUE(o.files_to_lint[0].is_stdin);
    EXPECT_FALSE(o.has_multiple_stdin);
  }

  {
    Options o = parse_options_no_errors({"one.js", "--stdin"});
    ASSERT_EQ(o.files_to_lint.size(), 2);
    EXPECT_TRUE(o.files_to_lint[1].is_stdin);
    EXPECT_FALSE(o.has_multiple_stdin);
  }

  {
    Options o = parse_options_no_errors({"-"});
    ASSERT_EQ(o.files_to_lint.size(), 1);
    EXPECT_TRUE(o.files_to_lint[0].is_stdin);
    EXPECT_FALSE(o.has_multiple_stdin);
  }
}

TEST(Test_Options, is_stdin_emplaced_only_once) {
  {
    Options o = parse_options_no_errors({"--stdin", "one.js", "-", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 3);
    EXPECT_TRUE(o.has_multiple_stdin);
  }
  {
    Options o = parse_options_no_errors({"one.js", "-", "two.js", "-"});
    ASSERT_EQ(o.files_to_lint.size(), 3);
    EXPECT_TRUE(o.has_multiple_stdin);
  }
}

TEST(Test_Options, single_hyphen_is_argument) {
  {
    Options o = parse_options_no_errors({"one.js", "-", "two.js"});
    ASSERT_EQ(o.files_to_lint.size(), 3);
  }
}

TEST(Test_Options, print_help) {
  {
    Options o = parse_options_no_errors({"--help"});
    EXPECT_TRUE(o.help);
  }

  {
    Options o = parse_options_no_errors({"--h"});
    EXPECT_TRUE(o.help);
  }

  {
    Options o = parse_options_no_errors({"-h"});
    EXPECT_TRUE(o.help);
  }
}

TEST(Test_Options, list_debug_apps) {
  Options o = parse_options_no_errors({"--debug-apps"});
  EXPECT_TRUE(o.list_debug_apps);
}

TEST(Test_Options, print_version) {
  {
    Options o = parse_options_no_errors({"--version"});
    EXPECT_TRUE(o.version);
  }

  {
    Options o = parse_options_no_errors({"--v"});
    EXPECT_TRUE(o.version);
  }

  {
    Options o = parse_options_no_errors({"-v"});
    EXPECT_TRUE(o.version);
  }
}

TEST(Test_Options, exit_fail_on) {
  {
    Options o = parse_options_no_errors({"--exit-fail-on=E0003", "file.js"});
    EXPECT_TRUE(
        o.exit_fail_on.is_present(Diag_Type::Diag_Assignment_To_Const_Variable))
        << "E0003 should cause failure";
    EXPECT_FALSE(o.exit_fail_on.is_present(
        Diag_Type::Diag_Big_Int_Literal_Contains_Decimal_Point))
        << "E0005 should not cause failure";
  }
}

TEST(Test_Options, invalid_vim_file_bufnr) {
  {
    Options o = parse_options({"--vim-file-bufnr=garbage", "file.js"});
    EXPECT_THAT(o.error_unrecognized_options, ElementsAreArray({"garbage"sv}));
  }

  {
    Options o = parse_options({"--vim-file-bufnr"});
    EXPECT_THAT(o.error_unrecognized_options,
                ElementsAreArray({"--vim-file-bufnr"sv}));
  }
}

TEST(Test_Options, no_following_filename_vim_file_bufnr) {
  {
    Options o = parse_options({"foo.js", "--vim-file-bufnr=1"});
    o.output_format = Output_Format::vim_qflist_json;

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: flag: '--vim-file-bufnr=1' should be followed by an "
              u8"input file name or --stdin\n");
  }
  {
    Options o =
        parse_options({"--vim-file-bufnr=1", "--vim-file-bufnr=2", "foo.js"});
    o.output_format = Output_Format::vim_qflist_json;

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: flag: '--vim-file-bufnr=1' should be followed by an "
              u8"input file name or --stdin\n");
  }
  {
    Options o =
        parse_options({"--vim-file-bufnr=1", "foo.js", "--vim-file-bufnr=2"});
    o.output_format = Output_Format::vim_qflist_json;

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: flag: '--vim-file-bufnr=2' should be followed by an "
              u8"input file name or --stdin\n");
  }
  {
    Options o = parse_options({"--vim-file-bufnr=1", "--vim-file-bufnr=2"});
    o.output_format = Output_Format::vim_qflist_json;

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: flag: '--vim-file-bufnr=1' should be followed by an "
              u8"input file name or --stdin\n"
              u8"warning: flag: '--vim-file-bufnr=2' should be followed by an "
              u8"input file name or --stdin\n");
  }
  {
    Options o = parse_options_no_errors({"--vim-file-bufnr=1",
                                         "foo.js"
                                         "--vim-file-bufnr=2",
                                         "--stdin"});
    o.output_format = Output_Format::vim_qflist_json;

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output, u8"");
  }
  {
    // test if the right argument gets inserted into the error message
    Options o =
        parse_options({"--vim-file-bufnr=11", "--output-format=vim-qflist-json",
                       "--vim-file-bufnr=22", "foo.js"});
    o.output_format = Output_Format::vim_qflist_json;

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: flag: '--vim-file-bufnr=11' should be followed by an "
              u8"input file name or --stdin\n");
  }
}

TEST(Test_Options, using_vim_file_bufnr_without_format) {
  {
    for (const auto &format : {
             Output_Format::default_format,
             Output_Format::gnu_like,
             Output_Format::emacs_lisp,
         }) {
      Options o = parse_options_no_errors({"--vim-file-bufnr=1", "file.js"});
      o.output_format = format;

      Dumped_Errors errors = dump_errors(o);
      EXPECT_FALSE(errors.have_errors);
      EXPECT_EQ(errors.output,
                u8"warning: --output-format selected which doesn't use "
                u8"--vim-file-bufnr\n");
    }
  }
  {
    Options o = parse_options_no_errors({"--vim-file-bufnr=1", "file.js"});
    o.output_format = Output_Format::vim_qflist_json;

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output, u8"");
  }
}

TEST(Test_Options, using_vim_file_bufnr_in_lsp_mode) {
  {
    Options o = parse_options({"--lsp-server", "--vim-file-bufnr=1"});

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: ignoring --vim-file-bufnr in --lsp-server mode\n");
  }
  {
    Options o = parse_options({"--lsp-server", "--vim-file-bufnr=1", "foo.js"});

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: ignoring files given on command line in --lsp-server "
              u8"mode\n"
              u8"warning: ignoring --vim-file-bufnr in --lsp-server mode\n");
  }
}

TEST(Test_Options, using_language_in_lsp_mode) {
  {
    Options o = parse_options({"--lsp-server", "--language=javascript"});

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: ignoring --language in --lsp-server mode\n");
  }
  {
    Options o =
        parse_options({"--lsp-server", "--language=javascript", "foo.js"});

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: ignoring files given on command line in --lsp-server "
              u8"mode\n"
              u8"warning: ignoring --language in --lsp-server mode\n");
  }
}

TEST(Test_Options, invalid_option) {
  {
    Options o = parse_options({"--option-does-not-exist", "foo.js"});
    EXPECT_THAT(o.error_unrecognized_options,
                ElementsAreArray({"--option-does-not-exist"sv}));
    EXPECT_THAT(o.files_to_lint, IsEmpty());
  }

  {
    Options o = parse_options({"--debug-parse-vixxx", "foo.js"});
    EXPECT_THAT(o.error_unrecognized_options,
                ElementsAreArray({"--debug-parse-vixxx"sv}));
    EXPECT_THAT(o.files_to_lint, IsEmpty());
  }

  {
    Options o = parse_options({"--debug-parse-visits-xxx", "foo.js"});
    EXPECT_THAT(o.error_unrecognized_options,
                ElementsAreArray({"--debug-parse-visits-xxx"sv}));
    EXPECT_THAT(o.files_to_lint, IsEmpty());
  }

  {
    Options o = parse_options({"-version", "foo.js"});
    EXPECT_THAT(o.error_unrecognized_options, ElementsAreArray({"-version"sv}));
    EXPECT_THAT(o.files_to_lint, IsEmpty());
  }
}

TEST(Test_Options, dump_errors) {
  {
    Options o;
    o.error_unrecognized_options.clear();

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output, u8"");
  }

  {
    Options o;
    o.error_unrecognized_options.push_back("--bad-option");

    Dumped_Errors errors = dump_errors(o);
    EXPECT_TRUE(errors.have_errors);
    EXPECT_EQ(errors.output, u8"error: unrecognized option: --bad-option\n");
  }

  {
    Options o;

    Parsed_Diag_Code_List parsed_errors;
    parsed_errors.included_categories.emplace_back("banana");
    parsed_errors.excluded_codes.emplace_back("E9999");
    o.exit_fail_on.add(parsed_errors);

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: unknown error category: banana\n"
              u8"warning: unknown error code: E9999\n");
  }

  {
    Options o;
    o.exit_fail_on.add(Parsed_Diag_Code_List());

    Dumped_Errors errors = dump_errors(o);
    EXPECT_TRUE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"error: --exit-fail-on must be given at least one category or "
              u8"code\n");
  }

  {
    Options o;
    o.lsp_server = true;
    o.output_format = Output_Format::default_format;

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output, u8"");
  }

  {
    for (const auto &format : {
             /* default_format intentionally left out */
             Output_Format::gnu_like,
             Output_Format::vim_qflist_json,
         }) {
      Options o;
      o.lsp_server = true;
      o.output_format = format;

      Dumped_Errors errors = dump_errors(o);
      EXPECT_FALSE(errors.have_errors);
      EXPECT_EQ(errors.output,
                u8"warning: --output-format ignored with --lsp-server\n");
    }
  }

  {
    Options o;
    o.lsp_server = true;
    o.has_config_file = true;

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: --config-file ignored in --lsp-server mode\n");
  }

  {
    const File_To_Lint file = {
        .path = "file.js",
        .config_file = nullptr,
        .language = std::nullopt,
        .is_stdin = false,
        .vim_bufnr = std::optional<int>(),
    };

    Options o;
    o.lsp_server = true;
    o.files_to_lint.emplace_back(file);

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: ignoring files given on command line in "
              u8"--lsp-server mode\n");
  }

  {
    Options o;
    o.lsp_server = true;
    o.exit_fail_on.add(parse_diag_code_list("E0001"));

    Dumped_Errors errors = dump_errors(o);
    EXPECT_FALSE(errors.have_errors);
    EXPECT_EQ(errors.output,
              u8"warning: --exit-fail-on ignored with --lsp-server\n");
  }
}
}
}

// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew "strager" Glazar
//
// This file is part of quick-lint-js.
//
// quick-lint-js is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// quick-lint-js is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with quick-lint-js.  If not, see <https://www.gnu.org/licenses/>.
