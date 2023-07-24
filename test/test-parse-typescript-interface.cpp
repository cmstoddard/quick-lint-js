// Copyright (C) 2020  Matthew "strager" Glazar
// See end of file for extended copyright information.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quick-lint-js/array.h>
#include <quick-lint-js/cli/cli-location.h>
#include <quick-lint-js/container/concat.h>
#include <quick-lint-js/container/padded-string.h>
#include <quick-lint-js/diag-collector.h>
#include <quick-lint-js/diag-matcher.h>
#include <quick-lint-js/diag/diagnostic-types.h>
#include <quick-lint-js/dirty-set.h>
#include <quick-lint-js/fe/language.h>
#include <quick-lint-js/fe/parse.h>
#include <quick-lint-js/parse-support.h>
#include <quick-lint-js/port/char8.h>
#include <quick-lint-js/spy-visitor.h>
#include <string>
#include <string_view>
#include <vector>

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

namespace quick_lint_js {
namespace {
class Test_Parse_TypeScript_Interface : public Test_Parse_Expression {};

TEST_F(Test_Parse_TypeScript_Interface, not_supported_in_vanilla_javascript) {
  Parser_Options options;
  options.typescript = false;
  Test_Parser p(u8"interface I {}"_sv, options, capture_diags);
  p.parse_and_visit_module();
  EXPECT_THAT(p.visits, ElementsAreArray({
                            "visit_variable_declaration",   // I
                            "visit_enter_interface_scope",  // I
                            "visit_exit_interface_scope",   // I
                            "visit_end_of_module",
                        }));
  assert_diagnostics(
      p.code, p.errors,
      {
          u8"^^^^^^^^^ Diag_TypeScript_Interfaces_Not_Allowed_In_JavaScript"_diag,
      });
}

TEST_F(Test_Parse_TypeScript_Interface, empty_interface) {
  Test_Parser p(u8"interface I {}"_sv, typescript_options, capture_diags);
  p.parse_and_visit_module();
  EXPECT_THAT(p.visits, ElementsAreArray({
                            "visit_variable_declaration",   // I
                            "visit_enter_interface_scope",  // I
                            "visit_exit_interface_scope",   // I
                            "visit_end_of_module",
                        }));
  EXPECT_THAT(p.variable_declarations,
              ElementsAreArray({interface_decl(u8"I"_sv)}));
  EXPECT_THAT(p.errors, IsEmpty());
}

TEST_F(Test_Parse_TypeScript_Interface, interface_without_body) {
  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I"_sv,                                               //
        u8"^^^^^^^^^^^ Diag_Missing_Body_For_TypeScript_Interface"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // I
                              "visit_exit_interface_scope",   // I
                              "visit_end_of_module",
                          }));
  }

  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I extends Other"_sv,  //
        u8"^^^^^^^^^^^^^^^^^^^^^^^^^ Diag_Missing_Body_For_TypeScript_Interface"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // I
                              "visit_variable_type_use",      // J
                              "visit_exit_interface_scope",   // I
                              "visit_end_of_module",
                          }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, extends) {
  Test_Parser p(u8"interface I extends A {}"_sv, typescript_options,
                capture_diags);
  p.parse_and_visit_module();
  EXPECT_THAT(p.visits, ElementsAreArray({
                            "visit_variable_declaration",   // I
                            "visit_enter_interface_scope",  // I
                            "visit_variable_type_use",      // A
                            "visit_exit_interface_scope",   // I
                            "visit_end_of_module",
                        }));
  EXPECT_THAT(p.variable_uses, ElementsAreArray({u8"A"}));
  EXPECT_THAT(p.errors, IsEmpty());
}

TEST_F(Test_Parse_TypeScript_Interface, extends_interface_from_namespace) {
  {
    Test_Parser p(u8"interface I extends ns.A {}"_sv, typescript_options,
                  capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",    // I
                              "visit_enter_interface_scope",   // I
                              "visit_variable_namespace_use",  // ns
                              "visit_exit_interface_scope",    // I
                              "visit_end_of_module",
                          }));
    EXPECT_THAT(p.variable_uses, ElementsAreArray({u8"ns"}));
    EXPECT_THAT(p.errors, IsEmpty());
  }

  {
    Test_Parser p(u8"interface I extends ns.subns.A {}"_sv, typescript_options);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",    // I
                              "visit_enter_interface_scope",   // I
                              "visit_variable_namespace_use",  // ns
                              "visit_exit_interface_scope",    // I
                              "visit_end_of_module",
                          }));
    EXPECT_THAT(p.variable_uses, ElementsAreArray({u8"ns"_sv}));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, extends_multiple_things) {
  Test_Parser p(u8"interface I extends A, B, C {}"_sv, typescript_options,
                capture_diags);
  p.parse_and_visit_module();
  EXPECT_THAT(p.visits, ElementsAreArray({
                            "visit_variable_declaration",   // I
                            "visit_enter_interface_scope",  // I
                            "visit_variable_type_use",      // A
                            "visit_variable_type_use",      // B
                            "visit_variable_type_use",      // C
                            "visit_exit_interface_scope",   // I
                            "visit_end_of_module",
                        }));
  EXPECT_THAT(p.variable_uses, ElementsAreArray({u8"A", u8"B", u8"C"}));
  EXPECT_THAT(p.errors, IsEmpty());
}

TEST_F(Test_Parse_TypeScript_Interface, extends_generic) {
  Test_Parser p(u8"interface I extends A<B> {}"_sv, typescript_options);
  p.parse_and_visit_module();
  EXPECT_THAT(p.visits, ElementsAreArray({
                            "visit_variable_declaration",   // I
                            "visit_enter_interface_scope",  // I
                            "visit_variable_type_use",      // A
                            "visit_variable_type_use",      // B
                            "visit_exit_interface_scope",   // I
                            "visit_end_of_module",
                        }));
  EXPECT_THAT(p.variable_uses, ElementsAreArray({u8"A", u8"B"}));
}

TEST_F(Test_Parse_TypeScript_Interface, unclosed_interface_statement) {
  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { "_sv,                                 //
        u8"            ^ Diag_Unclosed_Interface_Block"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  //
                              "visit_exit_interface_scope",   //
                              "visit_end_of_module",
                          }));
  }

  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { property "_sv,                        //
        u8"            ^ Diag_Unclosed_Interface_Block"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  //
                              "visit_property_declaration",   // property
                              "visit_exit_interface_scope",   //
                              "visit_end_of_module",
                          }));
  }

  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { method() "_sv,                        //
        u8"            ^ Diag_Unclosed_Interface_Block"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  //
                              "visit_property_declaration",   // method
                              "visit_enter_function_scope",   // method
                              "visit_exit_function_scope",    // method
                              "visit_exit_interface_scope",   //
                              "visit_end_of_module",
                          }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface,
       interface_can_be_named_contextual_keyword) {
  for (String8 name : contextual_keywords - typescript_builtin_type_keywords -
                          typescript_special_type_keywords -
                          Dirty_Set<String8>{
                              u8"let",
                              u8"static",
                              u8"yield",
                          }) {
    Padded_String code(concat(u8"interface "_sv, name, u8" {}"_sv));
    SCOPED_TRACE(code);
    Test_Parser p(code.string_view(), typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // (name)
                              "visit_enter_interface_scope",  //
                              "visit_exit_interface_scope",
                          }));
    EXPECT_THAT(p.variable_declarations,
                ElementsAreArray({interface_decl(name)}));
  }
}

TEST_F(Test_Parse_TypeScript_Interface,
       interface_cannot_have_newline_after_interface_keyword) {
  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface\nI {}"_sv,                                               //
        u8"^^^^^^^^^ Diag_Newline_Not_Allowed_After_Interface_Keyword"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  //
                              "visit_exit_interface_scope",
                          }));
  }

  {
    // NOTE(strager): This example is interpreted differently in JavaScript than
    // in TypeScript.
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface\nI<T> {}"_sv,                                            //
        u8"^^^^^^^^^ Diag_Newline_Not_Allowed_After_Interface_Keyword"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  //
                              "visit_variable_declaration",   // T
                              "visit_exit_interface_scope",
                          }));
  }

  {
    // NOTE(strager): This example is interpreted differently in JavaScript than
    // in TypeScript.
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface\nI<T>\n{}"_sv,                                           //
        u8"^^^^^^^^^ Diag_Newline_Not_Allowed_After_Interface_Keyword"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  //
                              "visit_variable_declaration",   // T
                              "visit_exit_interface_scope",
                          }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface,
       interface_keyword_with_following_newline_is_variable_name) {
  {
    Test_Parser p(u8"interface\nI\n{}"_sv, typescript_options);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_use",       // interface
                              "visit_variable_use",       // I
                              "visit_enter_block_scope",  // {
                              "visit_exit_block_scope",   // }
                              "visit_end_of_module",
                          }));
    EXPECT_THAT(p.variable_uses, ElementsAreArray({u8"interface", u8"I"}));
  }

  {
    // NOTE(strager): This example is interpreted differently in JavaScript than
    // in TypeScript.
    Test_Parser p(u8"interface\nI<T> {}"_sv, javascript_options);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_use",  // interface
                              "visit_variable_use",  // I
                              "visit_variable_use",  // T
                              "visit_end_of_module",
                          }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, property_without_type) {
  {
    Test_Parser p(u8"interface I { a;b\nc }"_sv, typescript_options,
                  capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // I
                              "visit_property_declaration",   // a
                              "visit_property_declaration",   // b
                              "visit_property_declaration",   // c
                              "visit_exit_interface_scope",   // I
                              "visit_end_of_module",
                          }));
    EXPECT_THAT(p.property_declarations,
                ElementsAreArray({u8"a", u8"b", u8"c"}));
    EXPECT_THAT(p.errors, IsEmpty());
  }

  {
    Test_Parser p(u8"interface I { 'fieldName'; }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   //
                              "visit_enter_interface_scope",  //
                              "visit_property_declaration",   // 'fieldName'
                              "visit_exit_interface_scope",
                          }));
    EXPECT_THAT(p.property_declarations, ElementsAreArray({std::nullopt}));
  }

  {
    Test_Parser p(u8"interface I { 3.14; }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   //
                              "visit_enter_interface_scope",  //
                              "visit_property_declaration",   // 3.14
                              "visit_exit_interface_scope",
                          }));
    EXPECT_THAT(p.property_declarations, ElementsAreArray({std::nullopt}));
  }

  {
    Test_Parser p(u8"interface I { [x + y]; }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   //
                              "visit_enter_interface_scope",  //
                              "visit_variable_use",           // x
                              "visit_variable_use",           // y
                              "visit_property_declaration",   // (x + y)
                              "visit_exit_interface_scope",
                          }));
    EXPECT_THAT(p.property_declarations, ElementsAreArray({std::nullopt}));
    EXPECT_THAT(p.variable_uses, ElementsAreArray({u8"x", u8"y"}));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, optional_property) {
  {
    Test_Parser p(u8"interface I { fieldName?; }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // I
                              "visit_property_declaration",   // fieldName
                              "visit_exit_interface_scope",   // I
                          }));
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"fieldName"}));
  }

  {
    // Semicolon is required.
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { fieldName? otherField }"_sv,  //
        u8"                        ` Diag_Missing_Semicolon_After_Field"_diag,  //

        typescript_options);
    EXPECT_THAT(p.property_declarations,
                ElementsAreArray({u8"fieldName", u8"otherField"}));
  }

  {
    // ASI
    Test_Parser p(u8"interface I { fieldName?\notherField }"_sv,
                  typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.property_declarations,
                ElementsAreArray({u8"fieldName", u8"otherField"}));
  }

  {
    Test_Parser p(u8"interface I { [2 + 2]?; }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.property_declarations, ElementsAreArray({std::nullopt}));
  }

  {
    Test_Parser p(u8"interface I { 'prop'?; }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.property_declarations, ElementsAreArray({std::nullopt}));
  }

  {
    Test_Parser p(u8"interface I { method?(param); }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // I
                              "visit_property_declaration",   // method
                              "visit_enter_function_scope",   // method
                              "visit_variable_declaration",   // param
                              "visit_exit_function_scope",    // method
                              "visit_exit_interface_scope",   // I
                          }));
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"method"}));
  }

  {
    Test_Parser p(u8"interface I { field?; }"_sv, javascript_options,
                  capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"field"}));
    EXPECT_THAT(
        p.errors,
        ElementsAreArray({
            DIAG_TYPE(Diag_TypeScript_Interfaces_Not_Allowed_In_JavaScript),
        }))
        << "should parse optional field but not complain about it";
  }
}

TEST_F(Test_Parse_TypeScript_Interface,
       assignment_asserted_field_is_disallowed) {
  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { fieldName!: any; }"_sv,  //
        u8"                       ^ Diag_TypeScript_Assignment_Asserted_Fields_Not_Allowed_In_Interfaces"_diag,  //
        typescript_options);
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"fieldName"}));
  }

  {
    // Missing type annotation should not report two errors.
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { fieldName!; }"_sv,  //
        u8"                       ^ Diag_TypeScript_Assignment_Asserted_Fields_Not_Allowed_In_Interfaces"_diag,  //
        typescript_options);
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"fieldName"}));
  }

  {
    // Initializer should not report two errors.
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { fieldName!: any = init; }"_sv,  //
        u8"                       ^ Diag_TypeScript_Assignment_Asserted_Fields_Not_Allowed_In_Interfaces"_diag,  //
        typescript_options);
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"fieldName"}));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, field_with_type) {
  {
    Test_Parser p(u8"interface I { fieldName: FieldType; }"_sv,
                  typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // I
                              "visit_variable_type_use",      // FieldType
                              "visit_property_declaration",   // fieldName
                              "visit_exit_interface_scope",   // I
                          }));
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"fieldName"}));
    EXPECT_THAT(p.variable_uses, ElementsAreArray({u8"FieldType"}));
  }

  {
    // Semicolon is required.
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { fieldName: FieldType otherField }"_sv,  //
        u8"                                  ` Diag_Missing_Semicolon_After_Field"_diag,  //

        typescript_options);
    EXPECT_THAT(p.property_declarations,
                ElementsAreArray({u8"fieldName", u8"otherField"}));
  }

  {
    // ASI
    Test_Parser p(u8"interface I { fieldName: FieldType\notherField }"_sv,
                  typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.property_declarations,
                ElementsAreArray({u8"fieldName", u8"otherField"}));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, interface_with_methods) {
  {
    Test_Parser p(u8"interface Monster { eatMuffins(muffinCount); }"_sv,
                  typescript_options);
    p.parse_and_visit_statement();
    ASSERT_EQ(p.variable_declarations.size(), 2);
    EXPECT_EQ(p.variable_declarations[0].name, u8"Monster");
    EXPECT_EQ(p.variable_declarations[1].name, u8"muffinCount");

    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"eatMuffins"}));

    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // Monster
                              "visit_enter_interface_scope",  //
                              "visit_property_declaration",   // eatMuffins
                              "visit_enter_function_scope",   //
                              "visit_variable_declaration",   // muffinCount
                              "visit_exit_function_scope",    //
                              "visit_exit_interface_scope",
                          }));
  }

  {
    Test_Parser p(u8"interface I { get length(); }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"length"}));
  }

  {
    Test_Parser p(u8"interface I { set length(value); }"_sv,
                  typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"length"}));
  }

  {
    Test_Parser p(u8"interface I { a(); b(); c(); }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.property_declarations,
                ElementsAreArray({u8"a", u8"b", u8"c"}));
  }

  {
    Test_Parser p(u8"interface I { \"stringKey\"(); }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.property_declarations, ElementsAreArray({std::nullopt}));
  }

  {
    Test_Parser p(u8"interface I { [x + y](); }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.variable_uses, ElementsAreArray({u8"x", u8"y"}));
    EXPECT_THAT(p.property_declarations, ElementsAreArray({std::nullopt}));
  }

  {
    Test_Parser p(u8"interface Getter<T> { get(): T; }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // Getter
                              "visit_enter_interface_scope",  // {
                              "visit_variable_declaration",   // T
                              "visit_property_declaration",   // get
                              "visit_enter_function_scope",   //
                              "visit_variable_type_use",      // T
                              "visit_exit_function_scope",    //
                              "visit_exit_interface_scope",   // }
                          }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, interface_with_index_signature) {
  {
    Test_Parser p(u8"interface I { [key: KeyType]: ValueType; }"_sv,
                  typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",         // I
                              "visit_enter_interface_scope",        // I
                              "visit_enter_index_signature_scope",  //
                              "visit_variable_type_use",            // KeyType
                              "visit_variable_declaration",         // key
                              "visit_variable_type_use",            // ValueType
                              "visit_exit_index_signature_scope",   //
                              "visit_exit_interface_scope",         // I
                          }));
    EXPECT_THAT(p.variable_uses,
                ElementsAreArray({u8"KeyType", u8"ValueType"}));
    // TODO(strager): We probably should create a new kind of variable instead
    // of 'parameter'.
    EXPECT_THAT(p.variable_declarations,
                ElementsAreArray({interface_decl(u8"I"_sv),
                                  index_signature_param_decl(u8"key"_sv)}));
  }

  {
    Test_Parser p(u8"interface I { [key: KeyType]: ValueType; }"_sv,
                  javascript_options, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",         // I
                              "visit_enter_interface_scope",        // I
                              "visit_enter_index_signature_scope",  //
                              "visit_variable_type_use",            // KeyType
                              "visit_variable_declaration",         // key
                              "visit_variable_type_use",            // ValueType
                              "visit_exit_index_signature_scope",   //
                              "visit_exit_interface_scope",         // I
                          }));
    EXPECT_THAT(
        p.errors,
        ElementsAreArray({
            DIAG_TYPE(Diag_TypeScript_Interfaces_Not_Allowed_In_JavaScript),
        }))
        << "should parse index signature and not complain about it";
  }
}

TEST_F(Test_Parse_TypeScript_Interface, index_signature_requires_type) {
  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { [key: KeyType]; }"_sv,  //
        u8"                            ` Diag_TypeScript_Index_Signature_Needs_Type"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",         // I
                              "visit_enter_interface_scope",        // I
                              "visit_enter_index_signature_scope",  //
                              "visit_variable_type_use",            // KeyType
                              "visit_variable_declaration",         // key
                              "visit_exit_index_signature_scope",   //
                              "visit_exit_interface_scope",         // I
                          }));
  }

  {
    // ASI
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { [key: KeyType]\n  method(); }"_sv,  //
        u8"                            ` Diag_TypeScript_Index_Signature_Needs_Type"_diag,  //

        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",         // I
                              "visit_enter_interface_scope",        // I
                              "visit_enter_index_signature_scope",  //
                              "visit_variable_type_use",            // KeyType
                              "visit_variable_declaration",         // key
                              "visit_exit_index_signature_scope",   //
                              "visit_property_declaration",         // method
                              "visit_enter_function_scope",         // method
                              "visit_exit_function_scope",          // method
                              "visit_exit_interface_scope",         // I
                          }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, index_signature_cannot_be_a_method) {
  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { [key: KeyType](param); }"_sv,  //
        u8"                            ^ Diag_TypeScript_Index_Signature_Cannot_Be_Method"_diag,  //

        typescript_options);
    EXPECT_THAT(p.visits,
                ElementsAreArray({
                    "visit_variable_declaration",         // I
                    "visit_enter_interface_scope",        // I
                    "visit_enter_index_signature_scope",  //
                    "visit_variable_type_use",            // KeyType
                    "visit_variable_declaration",         // key
                    // TODO(strager): Don't emit visit_property_declaration.
                    "visit_property_declaration",        //
                    "visit_enter_function_scope",        //
                    "visit_variable_declaration",        // param
                    "visit_exit_function_scope",         //
                    "visit_exit_index_signature_scope",  //
                    "visit_exit_interface_scope",        // I
                }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, index_signature_requires_semicolon) {
  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { [key: KeyType]: ValueType method(); }"_sv,  //
        u8"                                       ` Diag_Missing_Semicolon_After_Index_Signature"_diag,  //

        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",         // I
                              "visit_enter_interface_scope",        // I
                              "visit_enter_index_signature_scope",  //
                              "visit_variable_type_use",            // KeyType
                              "visit_variable_declaration",         // key
                              "visit_variable_type_use",            // ValueType
                              "visit_exit_index_signature_scope",   //
                              "visit_property_declaration",         // method
                              "visit_enter_function_scope",         // method
                              "visit_exit_function_scope",          // method
                              "visit_exit_interface_scope",         // I
                          }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, interface_methods_cannot_have_bodies) {
  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { method() { x } }"_sv,  //
        u8"                       ^ Diag_Interface_Methods_Cannot_Contain_Bodies"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",       // I
                              "visit_enter_interface_scope",      //
                              "visit_property_declaration",       // method
                              "visit_enter_function_scope",       // method
                              "visit_enter_function_scope_body",  // method
                              "visit_variable_use",               // x
                              "visit_exit_function_scope",        // method
                              "visit_exit_interface_scope",       //
                              "visit_end_of_module",
                          }));
  }

  {
    Test_Parser p(u8"interface I { method() => { x } }"_sv, typescript_options,
                  capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(
        p.errors,
        ::testing::UnorderedElementsAreArray({
            // TODO(strager): Report only one diagnostic:
            // Diag_Interface_Methods_Cannot_Contain_Bodies on the '=>'.
            DIAG_TYPE(Diag_Functions_Or_Methods_Should_Not_Have_Arrow_Operator),
            DIAG_TYPE_OFFSETS(
                p.code, Diag_Interface_Methods_Cannot_Contain_Bodies,  //
                body_start, u8"interface I { method() => "_sv.size(), u8"{"_sv),
        }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, interface_with_keyword_property) {
  for (String8_View suffix : {u8""_sv, u8"?"_sv}) {
    for (String8_View keyword : keywords) {
      {
        Test_Parser p(
            concat(u8"interface I { "_sv, keyword, suffix, u8"(); }"_sv),
            typescript_options);
        SCOPED_TRACE(p.code);
        p.parse_and_visit_statement();
        EXPECT_THAT(p.property_declarations, ElementsAreArray({keyword}));
      }

      for (String8_View prefix : {u8"get"_sv, u8"set"_sv}) {
        Test_Parser p(concat(u8"interface I { "_sv, prefix, u8" "_sv, keyword,
                             suffix, u8"(); }"_sv),
                      typescript_options);
        SCOPED_TRACE(p.code);
        p.parse_and_visit_statement();
        EXPECT_THAT(p.property_declarations, ElementsAreArray({keyword}));
      }

      {
        Test_Parser p(concat(u8"interface I { "_sv, keyword, suffix, u8" }"_sv),
                      typescript_options);
        SCOPED_TRACE(p.code);
        p.parse_and_visit_statement();
        EXPECT_THAT(p.property_declarations, ElementsAreArray({keyword}));
      }

      {
        Test_Parser p(
            concat(u8"interface I { "_sv, keyword, suffix, u8"; }"_sv),
            typescript_options);
        SCOPED_TRACE(p.code);
        p.parse_and_visit_statement();
        EXPECT_THAT(p.property_declarations, ElementsAreArray({keyword}));
      }
    }

    for (String8_View keyword : strict_reserved_keywords) {
      String8 property = escape_first_character_in_keyword(keyword);
      for (String8_View prefix : {u8""_sv, u8"get"_sv, u8"set"_sv}) {
        Padded_String code(concat(u8"interface I { "_sv, prefix, u8" "_sv,
                                  property, suffix, u8"(); }"_sv));
        SCOPED_TRACE(code);
        Test_Parser p(code.string_view(), typescript_options);
        p.parse_and_visit_statement();
        EXPECT_THAT(p.property_declarations, ElementsAreArray({keyword}));
      }
    }
  }
}

TEST_F(Test_Parse_TypeScript_Interface, interface_with_number_methods) {
  {
    Test_Parser p(u8"interface Wat { 42.0(); }"_sv, typescript_options);
    p.parse_and_visit_statement();
    ASSERT_EQ(p.variable_declarations.size(), 1);
    EXPECT_EQ(p.variable_declarations[0].name, u8"Wat");

    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // Wat
                              "visit_enter_interface_scope",  //
                              "visit_property_declaration",   // 42.0
                              "visit_enter_function_scope",   //
                              "visit_exit_function_scope",    //
                              "visit_exit_interface_scope",
                          }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, interface_allows_stray_semicolons) {
  Test_Parser p(u8"interface I{ ; f() ; ; }"_sv, typescript_options);
  p.parse_and_visit_statement();
  EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"f"}));
}

TEST_F(Test_Parse_TypeScript_Interface, private_properties_are_not_allowed) {
  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { #method(); }"_sv,  //
        u8"              ^^^^^^^ Diag_Interface_Properties_Cannot_Be_Private"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  //
                              "visit_property_declaration",   // #method
                              "visit_enter_function_scope",   // #method
                              "visit_exit_function_scope",    // #method
                              "visit_exit_interface_scope",   //
                              "visit_end_of_module",
                          }));
  }

  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { #field; }"_sv,  //
        u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Private"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  //
                              "visit_property_declaration",   // #field
                              "visit_exit_interface_scope",   //
                              "visit_end_of_module",
                          }));
  }

  {
    Test_Parser p(u8"interface I { async static #method(); }"_sv,
                  typescript_options, capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  //
                              "visit_property_declaration",   // #method
                              "visit_enter_function_scope",   // #method
                              "visit_exit_function_scope",    // #method
                              "visit_exit_interface_scope",   //
                              "visit_end_of_module",
                          }));
    EXPECT_THAT(
        p.errors,
        ::testing::UnorderedElementsAreArray({
            DIAG_TYPE(Diag_Interface_Methods_Cannot_Be_Async),
            DIAG_TYPE(Diag_Interface_Properties_Cannot_Be_Static),
            DIAG_TYPE_OFFSETS(
                p.code, Diag_Interface_Properties_Cannot_Be_Private,  //
                property_name_or_private_keyword,
                u8"interface I { async static "_sv.size(), u8"#method"_sv),
        }));
  }

  {
    Test_Parser p(u8"interface I { readonly static #field; }"_sv,
                  typescript_options, capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  //
                              "visit_property_declaration",   // #field
                              "visit_exit_interface_scope",   //
                              "visit_end_of_module",
                          }));
    EXPECT_THAT(
        p.errors,
        ::testing::UnorderedElementsAreArray({
            DIAG_TYPE(Diag_Interface_Properties_Cannot_Be_Static),
            DIAG_TYPE_OFFSETS(
                p.code, Diag_Interface_Properties_Cannot_Be_Private,  //
                property_name_or_private_keyword,
                u8"interface I { readonly static "_sv.size(), u8"#field"_sv),
        }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, static_properties_are_not_allowed) {
  for (String8 property_name : Dirty_Set<String8>{u8"myProperty"} | keywords) {
    SCOPED_TRACE(out_string8(property_name));

    {
      Test_Parser p(
          concat(u8"interface I { static "_sv, property_name, u8"(); }"_sv),
          typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(p.visits, ElementsAreArray({
                                "visit_variable_declaration",   // I
                                "visit_enter_interface_scope",  //
                                "visit_property_declaration",   // property
                                "visit_enter_function_scope",   // property
                                "visit_exit_function_scope",    // property
                                "visit_exit_interface_scope",   //
                                "visit_end_of_module",
                            }));
      assert_diagnostics(
          p.code, p.errors,
          {
              u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,
          });
    }

    {
      Test_Parser p(
          concat(u8"interface I { static get "_sv, property_name, u8"(); }"_sv),
          typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(p.visits, ElementsAreArray({
                                "visit_variable_declaration",   // I
                                "visit_enter_interface_scope",  //
                                "visit_property_declaration",   // property
                                "visit_enter_function_scope",   // property
                                "visit_exit_function_scope",    // property
                                "visit_exit_interface_scope",   //
                                "visit_end_of_module",
                            }));
      assert_diagnostics(
          p.code, p.errors,
          {
              u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,
          });
    }

    {
      Test_Parser p(concat(u8"interface I { static set "_sv, property_name,
                           u8"(value); }"_sv),
                    typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(p.visits, ElementsAreArray({
                                "visit_variable_declaration",   // I
                                "visit_enter_interface_scope",  //
                                "visit_property_declaration",   // property
                                "visit_enter_function_scope",   // property
                                "visit_variable_declaration",   // value
                                "visit_exit_function_scope",    // property
                                "visit_exit_interface_scope",   //
                                "visit_end_of_module",
                            }));
      assert_diagnostics(
          p.code, p.errors,
          {
              u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,
          });
    }

    {
      Test_Parser p(
          concat(u8"interface I { static "_sv, property_name, u8"; }"_sv),
          typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(p.visits, ElementsAreArray({
                                "visit_variable_declaration",   // I
                                "visit_enter_interface_scope",  //
                                "visit_property_declaration",   // property
                                "visit_exit_interface_scope",   //
                                "visit_end_of_module",
                            }));
      assert_diagnostics(
          p.code, p.errors,
          {
              u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,
          });
    }

    // TODO(#736): Fix 'static readonly static'.
    if (property_name != u8"static") {
      Test_Parser p(concat(u8"interface I { static readonly "_sv, property_name,
                           u8"; }"_sv),
                    typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(p.visits, ElementsAreArray({
                                "visit_variable_declaration",   // I
                                "visit_enter_interface_scope",  //
                                "visit_property_declaration",   // property
                                "visit_exit_interface_scope",   //
                                "visit_end_of_module",
                            }));
      assert_diagnostics(
          p.code, p.errors,
          {
              u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,
          });
    }

    {
      Test_Parser p(concat(u8"interface I { static async\n "_sv, property_name,
                           u8"(); }"_sv),
                    typescript_options, capture_diags);
      p.parse_and_visit_module();
      assert_diagnostics(
          p.code, p.errors,
          {
              u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,
          });
    }

    {
      // ASI doesn't activate after 'static'.
      // TODO(strager): Is this a bug in the TypeScript compiler?
      Test_Parser p(
          concat(u8"interface I { static\n"_sv, property_name, u8"(); }"_sv),
          typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(p.property_declarations, ElementsAreArray({property_name}));
      assert_diagnostics(
          p.code, p.errors,
          {
              u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,
          });
    }

    {
      // ASI doesn't activate after 'static'.
      // TODO(strager): Is this a bug in the TypeScript compiler?
      Test_Parser p(
          concat(u8"interface I { static\n"_sv, property_name, u8"; }"_sv),
          typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(p.property_declarations, ElementsAreArray({property_name}));
      assert_diagnostics(
          p.code, p.errors,
          {
              u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,
          });
    }
  }

  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { static field\n method(); }"_sv,  //
        u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,  //

        typescript_options);
  }

  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { static field\n ['methodName'](); }"_sv,  //
        u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,  //

        typescript_options);
  }

  {
    Test_Parser p(u8"interface I { static field? method(); }"_sv,
                  typescript_options, capture_diags);
    p.parse_and_visit_module();
    EXPECT_THAT(
        p.errors,
        ::testing::UnorderedElementsAreArray({
            DIAG_TYPE_OFFSETS(
                p.code, Diag_Interface_Properties_Cannot_Be_Static,  //
                static_keyword, u8"interface I { "_sv.size(), u8"static"_sv),
            DIAG_TYPE(Diag_Missing_Semicolon_After_Field),
        }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, async_methods_are_not_allowed) {
  for (String8 method_name : Dirty_Set<String8>{u8"method"} | keywords) {
    SCOPED_TRACE(out_string8(method_name));

    {
      Test_Parser p(
          concat(u8"interface I { async "_sv, method_name, u8"(); }"_sv),
          typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(p.visits, ElementsAreArray({
                                "visit_variable_declaration",   // I
                                "visit_enter_interface_scope",  //
                                "visit_property_declaration",   // method
                                "visit_enter_function_scope",   // method
                                "visit_exit_function_scope",    // method
                                "visit_exit_interface_scope",   //
                                "visit_end_of_module",
                            }));
      assert_diagnostics(
          p.code, p.errors,
          {
              u8"              ^^^^^ Diag_Interface_Methods_Cannot_Be_Async"_diag,
          });
    }

    {
      // ASI activates after 'async'.
      Test_Parser p(
          concat(u8"interface I { async\n"_sv, method_name, u8"(); }"_sv),
          typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(p.property_declarations, ElementsAre(u8"async", method_name));
      EXPECT_THAT(p.errors, IsEmpty());
    }
  }
}

TEST_F(Test_Parse_TypeScript_Interface, generator_methods_are_not_allowed) {
  for (String8 method_name : Dirty_Set<String8>{u8"method"} | keywords) {
    SCOPED_TRACE(out_string8(method_name));

    {
      Test_Parser p(concat(u8"interface I { *"_sv, method_name, u8"(); }"_sv),
                    typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(p.visits, ElementsAreArray({
                                "visit_variable_declaration",   // I
                                "visit_enter_interface_scope",  //
                                "visit_property_declaration",   // method
                                "visit_enter_function_scope",   // method
                                "visit_exit_function_scope",    // method
                                "visit_exit_interface_scope",   //
                                "visit_end_of_module",
                            }));
      assert_diagnostics(
          p.code, p.errors,
          {
              u8"              ^ Diag_Interface_Methods_Cannot_Be_Generators"_diag,
          });
    }

    {
      Test_Parser p(
          concat(u8"interface I { static *"_sv, method_name, u8"(); }"_sv),
          typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(
          p.errors,
          ::testing::UnorderedElementsAreArray({
              DIAG_TYPE(Diag_Interface_Properties_Cannot_Be_Static),
              DIAG_TYPE_OFFSETS(
                  p.code, Diag_Interface_Methods_Cannot_Be_Generators,  //
                  star, u8"interface I { static "_sv.size(), u8"*"_sv),
          }));
    }

    {
      Test_Parser p(
          concat(u8"interface I { async *"_sv, method_name, u8"(); }"_sv),
          typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(
          p.errors,
          ::testing::UnorderedElementsAreArray({
              DIAG_TYPE(Diag_Interface_Methods_Cannot_Be_Async),
              DIAG_TYPE_OFFSETS(
                  p.code, Diag_Interface_Methods_Cannot_Be_Generators,  //
                  star, u8"interface I { async "_sv.size(), u8"*"_sv),
          }));
    }
  }
}

TEST_F(Test_Parse_TypeScript_Interface,
       static_async_methods_are_definitely_not_allowed) {
  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { static async method(); }"_sv,  //
        u8"                     ^^^^^ Diag_Interface_Methods_Cannot_Be_Async"_diag,  //
        u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,  //

        typescript_options);
  }

  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { async static method(); }"_sv,  //
        u8"                    ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,  //
        u8"              ^^^^^ Diag_Interface_Methods_Cannot_Be_Async"_diag,  //

        typescript_options);
  }

  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { async static *method(); }"_sv,  //
        u8"                           ^ Diag_Interface_Methods_Cannot_Be_Generators"_diag,  //
        u8"                    ^^^^^^ Diag_Interface_Properties_Cannot_Be_Static"_diag,  //
        u8"              ^^^^^ Diag_Interface_Methods_Cannot_Be_Async"_diag,  //

        typescript_options);
  }
}

TEST_F(Test_Parse_TypeScript_Interface, field_initializers_are_not_allowed) {
  for (String8 field_name : Dirty_Set<String8>{u8"field"} | keywords) {
    SCOPED_TRACE(out_string8(field_name));

    {
      Test_Parser p(concat(u8"interface I { "_sv, field_name, u8" = y; }"_sv),
                    typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(p.visits, ElementsAreArray({
                                "visit_variable_declaration",   // I
                                "visit_enter_interface_scope",  //
                                "visit_variable_use",           // y
                                "visit_property_declaration",   // field_name
                                "visit_exit_interface_scope",   //
                                "visit_end_of_module",
                            }));
      EXPECT_THAT(
          p.errors,
          ElementsAreArray({
              DIAG_TYPE_OFFSETS(
                  p.code, Diag_Interface_Fields_Cannot_Have_Initializers,  //
                  equal, (u8"interface I { " + field_name + u8" ").size(),
                  u8"="_sv),
          }));
    }

    {
      Test_Parser p(
          concat(u8"interface I { static "_sv, field_name, u8" = y; }"_sv),
          typescript_options, capture_diags);
      p.parse_and_visit_module();
      EXPECT_THAT(
          p.errors,
          ::testing::UnorderedElementsAreArray({
              DIAG_TYPE(Diag_Interface_Properties_Cannot_Be_Static),
              DIAG_TYPE_OFFSETS(
                  p.code, Diag_Interface_Fields_Cannot_Have_Initializers,  //
                  equal,
                  (u8"interface I { static " + field_name + u8" ").size(),
                  u8"="_sv),
          }));
    }
  }

  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { 'fieldName' = init; }"_sv,  //
        u8"                          ^ Diag_Interface_Fields_Cannot_Have_Initializers"_diag,  //

        typescript_options);
  }

  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"interface I { fieldName: typeName = init; }"_sv,  //
        u8"                                  ^ Diag_Interface_Fields_Cannot_Have_Initializers"_diag,  //

        typescript_options);
  }
}

TEST_F(Test_Parse_TypeScript_Interface,
       interface_named_await_in_async_function) {
  {
    Test_Parser p(u8"interface await {}"_sv, typescript_options);
    p.parse_and_visit_statement();
  }

  {
    Test_Parser p(
        u8"function f() {"
        u8"interface await {}"
        u8"}"_sv,
        typescript_options);
    p.parse_and_visit_statement();
  }

  {
    Spy_Visitor p = test_parse_and_visit_module(
        u8"async function g() { interface await {} }"_sv,  //
        u8"                               ^^^^^ Diag_Cannot_Declare_Await_In_Async_Function"_diag,  //

        typescript_options);
  }
}

TEST_F(Test_Parse_TypeScript_Interface, call_signature) {
  {
    Test_Parser p(u8"interface I { (param); }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // I
                              // TODO(strager): Emit something other than
                              // visit_property_declaration instead?
                              "visit_property_declaration",  // (call signature)
                              "visit_enter_function_scope",  // (call signature)
                              "visit_variable_declaration",  // param
                              "visit_exit_function_scope",   // (call signature)
                              "visit_exit_interface_scope",  // I
                          }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface,
       call_signature_after_invalid_field_with_newline) {
  {
    Test_Parser p(
        u8"interface I {\n"
        u8"  field!\n"
        u8"  (param);\n"
        u8"}"_sv,
        typescript_options, capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // I
                              "visit_property_declaration",   // field
                              // TODO(strager): Emit something other than
                              // visit_property_declaration instead?
                              "visit_property_declaration",  // (call signature)
                              "visit_enter_function_scope",  // (call signature)
                              "visit_variable_declaration",  // param
                              "visit_exit_function_scope",   // (call signature)
                              "visit_exit_interface_scope",  // I
                          }));
    assert_diagnostics(
        p.code, p.errors,
        {
            u8"                      ^ Diag_TypeScript_Assignment_Asserted_Fields_Not_Allowed_In_Interfaces"_diag,
        });
  }
}

TEST_F(Test_Parse_TypeScript_Interface,
       call_signature_cannot_have_generator_star) {
  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { *(param); }"_sv,  //
        u8"              ^ Diag_Interface_Methods_Cannot_Be_Generators"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // I
                              // TODO(strager): Emit something other than
                              // visit_property_declaration instead?
                              "visit_property_declaration",  // (call signature)
                              "visit_enter_function_scope",  // (call signature)
                              "visit_variable_declaration",  // param
                              "visit_exit_function_scope",   // (call signature)
                              "visit_exit_interface_scope",  // I
                          }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, generic_call_signature) {
  {
    Test_Parser p(u8"interface I { <T>(param); }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // I
                              // TODO(strager): Emit something other than
                              // visit_property_declaration instead?
                              "visit_property_declaration",  // (call signature)
                              "visit_enter_function_scope",  // (call signature)
                              "visit_variable_declaration",  // T
                              "visit_variable_declaration",  // param
                              "visit_exit_function_scope",   // (call signature)
                              "visit_exit_interface_scope",  // I
                          }));
    EXPECT_THAT(p.variable_declarations,
                ElementsAreArray({interface_decl(u8"I"_sv),
                                  generic_param_decl(u8"T"_sv),
                                  func_param_decl(u8"param"_sv)}));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, generic_interface) {
  {
    Test_Parser p(u8"interface I<T> { field: T; }"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // I
                              "visit_variable_declaration",   // T
                              "visit_variable_type_use",      // T
                              "visit_property_declaration",   // field
                              "visit_exit_interface_scope",   // I
                          }));
    EXPECT_THAT(p.variable_declarations,
                ElementsAreArray(
                    {interface_decl(u8"I"_sv), generic_param_decl(u8"T"_sv)}));
  }

  {
    Test_Parser p(u8"interface I<T> extends T {}"_sv, typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // I
                              "visit_variable_declaration",   // T
                              "visit_variable_type_use",      // T
                              "visit_exit_interface_scope",   // I
                          }));
    EXPECT_THAT(p.variable_declarations,
                ElementsAreArray(
                    {interface_decl(u8"I"_sv), generic_param_decl(u8"T"_sv)}));
    EXPECT_THAT(p.variable_uses, ElementsAreArray({u8"T"_sv}));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, access_specifiers_are_not_allowed) {
  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { public method(); }"_sv,  //
        u8"              ^^^^^^ Diag_Interface_Properties_Cannot_Be_Explicitly_Public"_diag,  //
        typescript_options);
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"method"}));
  }

  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { protected method(); }"_sv,  //
        u8"              ^^^^^^^^^ Diag_Interface_Properties_Cannot_Be_Protected"_diag,  //

        typescript_options);
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"method"}));
  }

  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { private method(); }"_sv,  //
        u8"              ^^^^^^^ Diag_Interface_Properties_Cannot_Be_Private"_diag,  //
        typescript_options);
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"method"}));
  }
}

TEST_F(Test_Parse_TypeScript_Interface, static_blocks_are_not_allowed) {
  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { static { console.log('hello'); } }"_sv,  //
        u8"              ^^^^^^ Diag_TypeScript_Interfaces_Cannot_Contain_Static_Blocks"_diag,  //

        typescript_options);
    EXPECT_THAT(p.property_declarations, IsEmpty());
    EXPECT_THAT(p.variable_uses, ElementsAreArray({u8"console"}));
  }
}

TEST_F(Test_Parse_TypeScript_Interface,
       type_annotations_dont_add_extra_diagnostic_in_javascript) {
  {
    Test_Parser p(u8"interface I<T> { method(): Type; }"_sv, javascript_options,
                  capture_diags);
    p.parse_and_visit_statement();
    EXPECT_THAT(
        p.errors,
        ElementsAreArray({
            DIAG_TYPE(Diag_TypeScript_Interfaces_Not_Allowed_In_JavaScript),
        }))
        << "Diag_TypeScript_Type_Annotations_Not_Allowed_In_JavaScript should "
           "not be reported";
  }
}

TEST_F(Test_Parse_TypeScript_Interface, method_requires_semicolon_or_asi) {
  {
    Test_Parser p(
        u8"interface I {\n"
        u8"  f()\n"      // ASI
        u8"  g() }"_sv,  // ASI
        typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // {
                              "visit_property_declaration",   // f
                              "visit_enter_function_scope",   // f
                              "visit_exit_function_scope",    // f
                              "visit_property_declaration",   // g
                              "visit_enter_function_scope",   // g
                              "visit_exit_function_scope",    // g
                              "visit_exit_interface_scope",   // }
                          }));
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"f", u8"g"}));
  }

  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { f() g(); }"_sv,  //
        u8"                 ` Diag_Missing_Semicolon_After_Interface_Method"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // {
                              "visit_property_declaration",   // f
                              "visit_enter_function_scope",   // f
                              "visit_exit_function_scope",    // f
                              "visit_property_declaration",   // g
                              "visit_enter_function_scope",   // g
                              "visit_exit_function_scope",    // g
                              "visit_exit_interface_scope",   // }
                          }));
    EXPECT_THAT(p.property_declarations, ElementsAreArray({u8"f", u8"g"}));
  }
}

TEST_F(Test_Parse_TypeScript_Interface,
       abstract_properties_are_not_allowed_in_interfaces) {
  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { abstract myField; }"_sv,  //
        u8"              ^^^^^^^^ Diag_Abstract_Property_Not_Allowed_In_Interface"_diag,  //
        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // {
                              "visit_property_declaration",   // myField
                              "visit_exit_interface_scope",   // }
                          }));
  }

  {
    Spy_Visitor p = test_parse_and_visit_statement(
        u8"interface I { abstract myMethod(); }"_sv,  //
        u8"              ^^^^^^^^ Diag_Abstract_Property_Not_Allowed_In_Interface"_diag,  //

        typescript_options);
    EXPECT_THAT(p.visits, ElementsAreArray({
                              "visit_variable_declaration",   // I
                              "visit_enter_interface_scope",  // {
                              "visit_property_declaration",   // myMethod
                              "visit_enter_function_scope",   // myMethod
                              "visit_exit_function_scope",    // myMethod
                              "visit_exit_interface_scope",   // }
                          }));
  }
}

TEST_F(Test_Parse_TypeScript_Interface,
       interface_keyword_with_escape_sequence) {
  {
    Test_Parser p(
        u8"interface A {\n"_sv
        u8"  \\u{63}onstructor();}"_sv,
        typescript_options);
    p.parse_and_visit_statement();
    EXPECT_THAT(p.errors, IsEmpty());
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
