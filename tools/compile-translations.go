// Copyright (C) 2020  Matthew "strager" Glazar
// See end of file for extended copyright information.

// quick-lint-js uses a custom format to store translations. Originally,
// quick-lint-js used GNU gettext's MO format, but it had a number of issues:
//
// * Storing multiple locale requires storing source strings one per
//   locale. This increases file size.
// * The hash function is weak, resulting in many hash collisions.
// * The hash function is imperfect, forcing lookup to compare a matching source
//   string in full for every translation.
// * MO files can be little endian or big endian, complicating lookup.
//
// Our custom format addresses these issues:
//
// * All locales are merged into a single data structure.
// * Lookups happen at compile time, never at run time.
// * The data is always native endian.
//
// The custom format has four parts:
// * the compile-time lookup table
// * the mapping table
// * the string table
// * the locale table
// * some constants
//
// These are the constants (C++ code):
//
//     std::uint32_t locale_count = /* ... */;
//     std::uint16_t mapping_table_size = /* ... */;
//     std::size_t strings_size = /* ... */;
//     std::size_t locale_table_size = /* ... */;
//
// The lookup table is compile-time-only. It is used to convert an untranslated
// string into an index into the mapping table. The lookup table is a sorted
// list of the untranslated strings. The sorting makes output deterministic and
// also enables binary searching.
//
// The mapping table and the locale table are run-time-only. They look like this
// (C++ code):
//
//     struct mapping_entry {
//       std::uint32_t string_offsets[locale_count + 1];
//     };
//     mapping_entry mapping_table[mapping_table_size];
//
//     char locale_table[locale_table_size] =
//       "en_US\0"
//       "de_DE\0"
//       /* ... */
//       "";  // C++ adds an extra null byte for us.
//
// mapping_entry::string_offsets[i] corresponds to the i-th locale listed in
// locale_table.
//
// mapping_entry::string_offsets[locale_count] refers to the original
// (untranslated) string.
//
// Entry 0 of the mapping table is unused.
//
// The string table contains 0-terminated UTF-8 strings. String sizes can be
// computed by calculating the difference between the first 0 byte starting at
// the string offset and the string offset.

package main

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strings"
)

const poDirectory string = "po"

func main() {
	poFiles, err := ListPOFiles()
	if err != nil {
		log.Fatal(err)
	}

	locales := map[string][]TranslationEntry{}
	for _, poFilePath := range poFiles {
		gmo, err := POFileToGMO(poFilePath)
		if err != nil {
			log.Fatal(err)
		}
		locales[POPathToLocaleName(poFilePath)] = ExtractGMOStrings(gmo)
	}

	sourceGMO, err := POTFileToGMO(filepath.Join(poDirectory, "messages.pot"))
	if err != nil {
		log.Fatal(err)
	}
	locales[""] = ExtractGMOStrings(sourceGMO)

	table := CreateTranslationTable(locales)
	if err := WriteTranslationTableHeader(&table, "src/quick-lint-js/translation-table-generated.h"); err != nil {
		log.Fatal(err)
	}
	if err := WriteTranslationTableSource(&table, "src/translation-table-generated.cpp"); err != nil {
		log.Fatal(err)
	}
	if err := WriteTranslationTest(locales, "test/quick-lint-js/test-translation-table-generated.h"); err != nil {
		log.Fatal(err)
	}
}

func writeFileHeader(writer *bufio.Writer) {
	writer.WriteString("// Code generated by tools/compile-translations.go. DO NOT EDIT.\n")
	writer.WriteString("// source: po/*.po\n\n")
	writer.WriteString("// Copyright (C) 2020  Matthew \"strager\" Glazar\n")
	writer.WriteString("// See end of file for extended copyright information.\n")
}

func ListPOFiles() ([]string, error) {
	filesInPODirectory, err := ioutil.ReadDir(poDirectory)
	if err != nil {
		return nil, err
	}
	var poFiles []string
	for _, fileInfo := range filesInPODirectory {
		if filepath.Ext(fileInfo.Name()) == ".po" {
			poFiles = append(poFiles, filepath.Join(poDirectory, fileInfo.Name()))
		}
	}
	// Sort locales to make builds reproducible.
	sort.Strings(poFiles)
	return poFiles, nil
}

func POPathToLocaleName(path string) string {
	return strings.TrimSuffix(filepath.Base(path), filepath.Ext(path))
}

func POFileToGMO(poFilePath string) ([]byte, error) {
	process := exec.Command(
		"msgfmt",
		"--output-file=-",
		"--",
		poFilePath,
	)
	var gmo bytes.Buffer
	process.Stdout = &gmo
	process.Stderr = os.Stderr
	if err := process.Start(); err != nil {
		return nil, err
	}
	if err := process.Wait(); err != nil {
		return nil, err
	}
	return gmo.Bytes(), nil
}

func POTFileToGMO(potFilePath string) ([]byte, error) {
	potToPOProcess := exec.Command(
		"msgen",
		"--output-file=-",
		"--",
		potFilePath,
	)
	var po bytes.Buffer
	potToPOProcess.Stdout = &po
	potToPOProcess.Stderr = os.Stderr
	if err := potToPOProcess.Run(); err != nil {
		return nil, err
	}

	poToGMOProcess := exec.Command(
		"msgfmt",
		"--output-file=-",
		"-",
	)
	var gmo bytes.Buffer
	poToGMOProcess.Stdin = &po
	poToGMOProcess.Stdout = &gmo
	poToGMOProcess.Stderr = os.Stderr
	if err := poToGMOProcess.Run(); err != nil {
		return nil, err
	}
	return gmo.Bytes(), nil
}

type TranslationEntry struct {
	Untranslated []byte
	Translated   []byte
}

func (entry *TranslationEntry) IsMetadata() bool {
	return len(entry.Untranslated) == 0
}

func ExtractGMOStrings(gmoData []byte) []TranslationEntry {
	var magic uint32 = binary.LittleEndian.Uint32(gmoData[0:])
	var decode binary.ByteOrder
	if magic == 0x950412de {
		decode = binary.LittleEndian
	} else {
		decode = binary.BigEndian
	}

	stringAt := func(tableOffset uint32, index uint32) []byte {
		tableEntryOffset := tableOffset + index*8
		length := decode.Uint32(gmoData[tableEntryOffset+0:])
		offset := decode.Uint32(gmoData[tableEntryOffset+4:])
		return gmoData[offset : offset+length]
	}

	entries := []TranslationEntry{}
	stringCount := decode.Uint32(gmoData[8:])
	originalTableOffset := decode.Uint32(gmoData[12:])
	translatedTableOffset := decode.Uint32(gmoData[16:])
	for i := uint32(0); i < stringCount; i += 1 {
		entries = append(entries, TranslationEntry{
			Untranslated: stringAt(originalTableOffset, i),
			Translated:   stringAt(translatedTableOffset, i),
		})
	}
	return entries
}

// Return value is sorted with no duplicates.
//
// The returned slice always contains an empty string at the beginning.
func GetLocaleNames(locales map[string][]TranslationEntry) []string {
	localeNames := []string{""}
	for localeName := range locales {
		if localeName != "" {
			localeNames = append(localeNames, localeName)
		}
	}
	// Sort to make output deterministic.
	sort.Strings(localeNames)
	return localeNames
}

// Extracts .Untranslated from each TranslationEntry.
//
// Return value is sorted with no duplicates.
func GetAllUntranslated(locales map[string][]TranslationEntry) [][]byte {
	allUntranslated := [][]byte{}
	addUntranslated := func(untranslated []byte) {
		for _, existingUntranslated := range allUntranslated {
			foundDuplicate := bytes.Equal(existingUntranslated, untranslated)
			if foundDuplicate {
				return
			}
		}
		allUntranslated = append(allUntranslated, untranslated)
	}
	for _, localeTranslations := range locales {
		for _, translation := range localeTranslations {
			if !translation.IsMetadata() {
				addUntranslated(translation.Untranslated)
			}
		}
	}
	// Sort to make output deterministic.
	sort.Slice(allUntranslated, func(i int, j int) bool {
		return bytes.Compare(allUntranslated[i], allUntranslated[j]) < 0
	})
	return allUntranslated
}

type TranslationTable struct {
	ConstLookupTable []TranslationTableConstLookupEntry
	MappingTable     []TranslationTableMappingEntry
	StringTable      []byte
	Locales          []string
	LocaleTable      []byte
}

type TranslationTableConstLookupEntry struct {
	Untranslated []byte
}

type TranslationTableMappingEntry struct {
	// Key: index of locale in TranslationTable.Locales
	// Value: offset in TranslationTable.StringTable
	StringOffsets []uint32
}

func CreateTranslationTable(locales map[string][]TranslationEntry) TranslationTable {
	table := TranslationTable{}

	addStringToTable := func(stringToAdd []byte, outTable *[]byte) uint32 {
		offset := uint32(len(*outTable))
		*outTable = append(*outTable, stringToAdd...)
		*outTable = append(*outTable, 0)
		return offset
	}

	addString := func(stringToAdd []byte) uint32 {
		return addStringToTable(stringToAdd, &table.StringTable)
	}

	keys := GetAllUntranslated(locales)
	table.Locales = GetLocaleNames(locales)

	// Put the untranslated ("") locale last. This has two effects:
	// * When writing LocaleTable, we'll add an empty locale at the end,
	//   terminating the list. This terminator is how find_locales (C++)
	//   knows the bounds of the locale table.
	// * Untranslated strings are placed in
	//   hash_entry::string_offsets[locale_count].
	table.Locales = append(table.Locales[1:], table.Locales[0])

	for _, localeName := range table.Locales {
		addStringToTable([]byte(localeName), &table.LocaleTable)
	}

	table.ConstLookupTable = make([]TranslationTableConstLookupEntry, len(keys))
	for i, key := range keys {
		table.ConstLookupTable[i].Untranslated = key
	}

	table.StringTable = []byte{0}
	mappingTableSize := len(keys) + 1
	table.MappingTable = make([]TranslationTableMappingEntry, mappingTableSize)
	for i := 0; i < mappingTableSize; i += 1 {
		mappingEntry := &table.MappingTable[i]
		mappingEntry.StringOffsets = make([]uint32, len(table.Locales))
	}
	for localeIndex, localeName := range table.Locales {
		localeTranslations := locales[localeName]
		for _, translation := range localeTranslations {
			if !translation.IsMetadata() {
				index := table.FindMappingTableIndexForUntranslated(translation.Untranslated)
				table.MappingTable[index].StringOffsets[localeIndex] = addString(translation.Translated)
			}
		}
	}

	return table
}

// Returns an index into table.MappingTable.
// If originalString is bogus, returns -1.
func (table *TranslationTable) FindMappingTableIndexForUntranslated(originalString []byte) int {
	for i := range table.ConstLookupTable {
		if bytes.Equal(table.ConstLookupTable[i].Untranslated, originalString) {
			return i + 1
		}
	}
	return -1
}

func (table *TranslationTable) LookUpMappingByUntranslated(originalString []byte) *TranslationTableMappingEntry {
	index := table.FindMappingTableIndexForUntranslated(originalString)
	if index == -1 {
		return nil
	}
	return &table.MappingTable[index]
}

func (table *TranslationTable) ReadString(stringOffset uint32) []byte {
	stringData := table.StringTable[stringOffset:]
	stringLength := bytes.IndexByte(stringData, 0)
	return stringData[:stringLength]
}

func WriteTranslationTableHeader(table *TranslationTable, path string) error {
	outputFile, err := os.Create(path)
	if err != nil {
		log.Fatal(err)
	}
	defer outputFile.Close()
	writer := bufio.NewWriter(outputFile)

	writeFileHeader(writer)

	writer.WriteString(
		`
#ifndef QUICK_LINT_JS_TRANSLATION_TABLE_GENERATED_H
#define QUICK_LINT_JS_TRANSLATION_TABLE_GENERATED_H

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <quick-lint-js/assert.h>
#include <quick-lint-js/consteval.h>
#include <quick-lint-js/translation-table.h>
#include <string_view>

namespace quick_lint_js {
using namespace std::literals::string_view_literals;

`)
	fmt.Fprintf(writer, "constexpr std::uint32_t translation_table_locale_count = %d;\n", len(table.Locales)-1)
	fmt.Fprintf(writer, "constexpr std::uint16_t translation_table_mapping_table_size = %d;\n", len(table.MappingTable))
	fmt.Fprintf(writer, "constexpr std::size_t translation_table_string_table_size = %d;\n", len(table.StringTable))
	fmt.Fprintf(writer, "constexpr std::size_t translation_table_locale_table_size = %d;\n", len(table.LocaleTable))
	fmt.Fprintf(writer, "\n")

	writer.WriteString(
		`QLJS_CONSTEVAL std::uint16_t translation_table_const_look_up(
    std::string_view untranslated) {
  // clang-format off
  constexpr std::string_view const_lookup_table[] = {
`)
	for _, constLookupEntry := range table.ConstLookupTable {
		fmt.Fprintf(writer, "          \"")
		DumpStringLiteralBody(string(constLookupEntry.Untranslated), writer)
		writer.WriteString("\"sv,\n")
	}
	fmt.Fprintf(writer,
		`  };
  // clang-format on

  std::uint16_t table_size = std::uint16_t(std::size(const_lookup_table));
  for (std::uint16_t i = 0; i < table_size; ++i) {
    if (const_lookup_table[i] == untranslated) {
      return std::uint16_t(i + 1);
    }
  }

  // If you see an error with the following line, translation-table-generated.h
  // is out of date. Run tools/update-translator-sources to rebuild this file.
  QLJS_CONSTEXPR_ASSERT(false);

  return 0;
}
}

#endif

`)
	WriteCopyrightFooter(writer)

	if err := writer.Flush(); err != nil {
		log.Fatal(err)
	}
	return nil
}

func WriteTranslationTableSource(table *TranslationTable, path string) error {
	outputFile, err := os.Create(path)
	if err != nil {
		log.Fatal(err)
	}
	defer outputFile.Close()
	writer := bufio.NewWriter(outputFile)

	writeFileHeader(writer)

	writer.WriteString(
		`
#include <quick-lint-js/translation-table.h>

namespace quick_lint_js {
const translation_table translation_data =
    {
        .mapping_table =
            {
`)
	for _, mappingEntry := range table.MappingTable {
		writer.WriteString("                {")
		for i, stringOffset := range mappingEntry.StringOffsets {
			if i != 0 {
				writer.WriteString(", ")
			}
			fmt.Fprintf(writer, "%d", stringOffset)
		}
		writer.WriteString("},\n")
	}
	writer.WriteString(
		`            },

        // clang-format off
        .string_table =
`)
	DumpStringTable(table.StringTable, "            u8", writer)

	writer.WriteString(
		`,
        // clang-format on

        .locale_table =
`)

	DumpStringTable(table.LocaleTable, "            ", writer)

	writer.WriteString(
		`,
};
}

`)
	WriteCopyrightFooter(writer)

	if err := writer.Flush(); err != nil {
		log.Fatal(err)
	}
	return nil
}

func WriteTranslationTest(locales map[string][]TranslationEntry, path string) error {
	outputFile, err := os.Create(path)
	if err != nil {
		log.Fatal(err)
	}
	defer outputFile.Close()
	writer := bufio.NewWriter(outputFile)

	localeNames := GetLocaleNames(locales)
	allUntranslated := GetAllUntranslated(locales)

	// Returns the untranslated string if there is no translation.
	lookUpTranslation := func(localeName string, untranslated []byte) []byte {
		for _, entry := range locales[localeName] {
			if bytes.Equal(entry.Untranslated, untranslated) {
				return entry.Translated
			}
		}
		return untranslated
	}

	writeFileHeader(writer)

	fmt.Fprintf(writer,
		`
#ifndef QUICK_LINT_JS_TEST_TRANSLATION_TABLE_GENERATED_H
#define QUICK_LINT_JS_TEST_TRANSLATION_TABLE_GENERATED_H

#include <quick-lint-js/char8.h>
#include <quick-lint-js/translation.h>

namespace quick_lint_js {
// clang-format off
inline constexpr const char *test_locale_names[] = {
`)

	for _, localeName := range localeNames {
		fmt.Fprintf(writer, "    \"")
		DumpStringLiteralBody(localeName, writer)
		fmt.Fprintf(writer, "\",\n")
	}

	fmt.Fprintf(writer,
		`};
// clang-format on

struct translated_string {
  translatable_message translatable;
  const char8 *expected_per_locale[%d];
};

// clang-format off
inline constexpr translated_string test_translation_table[] = {
`, len(localeNames))

	for _, untranslated := range allUntranslated {
		fmt.Fprintf(writer, "    {\n        \"")
		DumpStringLiteralBody(string(untranslated), writer)
		fmt.Fprintf(writer, "\"_translatable,\n        {\n")
		for _, localeName := range localeNames {
			fmt.Fprintf(writer, "            u8\"")
			DumpStringLiteralBody(string(lookUpTranslation(localeName, untranslated)), writer)
			fmt.Fprintf(writer, "\",\n")
		}
		fmt.Fprintf(writer, "        },\n    },\n")
	}

	fmt.Fprintf(writer,
		`};
// clang-format on
}

#endif
`)
	WriteCopyrightFooter(writer)

	if err := writer.Flush(); err != nil {
		log.Fatal(err)
	}
	return nil
}

func WriteCopyrightFooter(writer *bufio.Writer) {
	writer.WriteString(
		`// quick-lint-js finds bugs in JavaScript programs.
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
`)
}

func DumpStringTable(strings []byte, linePrefix string, writer *bufio.Writer) {
	for len(strings) != 0 {
		writer.WriteString(linePrefix)
		writer.WriteRune('"')
		stringLength := bytes.IndexByte(strings, 0)
		s := string(strings[:stringLength])
		DumpStringLiteralBody(s, writer)
		strings = strings[stringLength+1:]
		if len(strings) != 0 {
			// C++ adds a \0 for us, so we don't need to add one ourselves.
			writer.WriteString(`\0`)
		}
		writer.WriteRune('"')
		if len(strings) != 0 {
			writer.WriteRune('\n')
		}
	}
}

func DumpStringLiteralBody(s string, writer *bufio.Writer) {
	for _, c := range s {
		if c < 0x20 || c >= 0x7f {
			if c >= 0x10000 {
				fmt.Fprintf(writer, `\U%08x`, c)
			} else {
				fmt.Fprintf(writer, `\u%04x`, c)
			}
		} else if c == '\\' || c == '"' {
			writer.WriteRune('\\')
			writer.WriteRune(c)
		} else {
			writer.WriteRune(c)
		}
	}
}

func HashFNV1a64(data []byte) uint64 {
	return HashFNV1a64WithOffsetBasis(data, 0xcbf29ce484222325)
}

func HashFNV1a64WithOffsetBasis(data []byte, offsetBasis uint64) uint64 {
	var hash uint64 = offsetBasis
	for _, b := range data {
		hash ^= uint64(b)
		hash *= 0x00000100_000001b3
	}
	return hash
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
