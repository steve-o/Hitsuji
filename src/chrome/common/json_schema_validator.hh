// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_JSON_SCHEMA_VALIDATOR_HH_
#define CHROME_COMMON_JSON_SCHEMA_VALIDATOR_HH_

#include <map>
#include <string>
#include <vector>

/* Boost noncopyable base class. */
#include <boost/utility.hpp>

#include "../../chromium/basictypes.hh"

namespace chromium {
class DictionaryValue;
class ListValue;
class StringValue;
class Value;
}

//==============================================================================
// This class implements a subset of JSON Schema.
// See: http://www.json.com/json-schema-proposal/ for more details.
//
// There is also an older JavaScript implementation of the same functionality in
// chrome/renderer/resources/json_schema.js.
//
// The following features of JSON Schema are not implemented:
// - requires
// - unique
// - disallow
// - union types (but replaced with 'choices')
// - number.maxDecimal
// - string.pattern
//
// The following properties are not applicable to the interface exposed by
// this class:
// - options
// - readonly
// - title
// - description
// - format
// - default
// - transient
// - hidden
//
// There are also these departures from the JSON Schema proposal:
// - null counts as 'unspecified' for optional values
// - added the 'choices' property, to allow specifying a list of possible types
//   for a value
// - by default an "object" typed schema does not allow additional properties.
//   if present, "additionalProperties" is to be a schema against which all
//   additional properties will be validated.
//==============================================================================
class JSONSchemaValidator : boost::noncopyable {
 public:
  // Details about a validation error.
  struct Error {
    Error();

    explicit Error(const std::string& message);

    Error(const std::string& path, const std::string& message);

    // The path to the location of the error in the JSON structure.
    std::string path;

    // An english message describing the error.
    std::string message;
  };

  // Error messages.
  static const char kUnknownTypeReference[];
  static const char kInvalidChoice[];
  static const char kInvalidEnum[];
  static const char kObjectPropertyIsRequired[];
  static const char kUnexpectedProperty[];
  static const char kArrayMinItems[];
  static const char kArrayMaxItems[];
  static const char kArrayItemRequired[];
  static const char kStringMinLength[];
  static const char kStringMaxLength[];
  static const char kStringPattern[];
  static const char kNumberMinimum[];
  static const char kNumberMaximum[];
  static const char kInvalidType[];

  // Classifies a Value as one of the JSON schema primitive types.
  static std::string GetJSONSchemaType(chromium::Value* value);

  // Utility methods to format error messages. The first method can have one
  // wildcard represented by '*', which is replaced with s1. The second method
  // can have two, which are replaced by s1 and s2.
  static std::string FormatErrorMessage(const std::string& format,
                                        const std::string& s1);
  static std::string FormatErrorMessage(const std::string& format,
                                        const std::string& s1,
                                        const std::string& s2);

  // Creates a validator for the specified schema.
  //
  // NOTE: This constructor assumes that |schema| is well formed and valid.
  // Errors will result in CHECK at runtime; this constructor should not be used
  // with untrusted schemas.
  explicit JSONSchemaValidator(chromium::DictionaryValue* schema);

  // Creates a validator for the specified schema and user-defined types. Each
  // type must be a valid JSONSchema type description with an additional "id"
  // field. Schema objects in |schema| can refer to these types with the "$ref"
  // property.
  //
  // NOTE: This constructor assumes that |schema| and |types| are well-formed
  // and valid. Errors will result in CHECK at runtime; this constructor should
  // not be used with untrusted schemas.
  JSONSchemaValidator(chromium::DictionaryValue* schema, chromium::ListValue* types);

  ~JSONSchemaValidator();

  // Whether the validator allows additional items for objects and lists, beyond
  // those defined by their schema, by default.
  //
  // This setting defaults to false: all items in an instance list or object
  // must be defined by the corresponding schema.
  //
  // This setting can be overridden on individual object and list schemas by
  // setting the "additionalProperties" field.
  bool default_allow_additional_properties() const {
    return default_allow_additional_properties_;
  }

  void set_default_allow_additional_properties(bool val) {
    default_allow_additional_properties_ = val;
  }

  // Returns any errors from the last call to to Validate().
  const std::vector<Error>& errors() const {
    return errors_;
  }

  // Validates a JSON value. Returns true if the instance is valid, false
  // otherwise. If false is returned any errors are available from the errors()
  // getter.
  bool Validate(chromium::Value* instance);

 private:
  typedef std::map<std::string, chromium::DictionaryValue*> TypeMap;

  // Each of the below methods handle a subset of the validation process. The
  // path paramater is the path to |instance| from the root of the instance tree
  // and is used in error messages.

  // Validates any instance node against any schema node. This is called for
  // every node in the instance tree, and it just decides which of the more
  // detailed methods to call.
  void Validate(chromium::Value* instance, chromium::DictionaryValue* schema,
                const std::string& path);

  // Validates a node against a list of possible schemas. If any one of the
  // schemas match, the node is valid.
  void ValidateChoices(chromium::Value* instance, chromium::ListValue* choices,
                       const std::string& path);

  // Validates a node against a list of exact primitive values, eg 42, "foobar".
  void ValidateEnum(chromium::Value* instance, chromium::ListValue* choices,
                    const std::string& path);

  // Validates a JSON object against an object schema node.
  void ValidateObject(chromium::DictionaryValue* instance,
                      chromium::DictionaryValue* schema,
                      const std::string& path);

  // Validates a JSON array against an array schema node.
  void ValidateArray(chromium::ListValue* instance, chromium::DictionaryValue* schema,
                     const std::string& path);

  // Validates a JSON array against an array schema node configured to be a
  // tuple. In a tuple, there is one schema node for each item expected in the
  // array.
  void ValidateTuple(chromium::ListValue* instance, chromium::DictionaryValue* schema,
                     const std::string& path);

  // Validate a JSON string against a string schema node.
  void ValidateString(chromium::StringValue* instance,
                      chromium::DictionaryValue* schema,
                      const std::string& path);

  // Validate a JSON number against a number schema node.
  void ValidateNumber(chromium::Value* instance,
                      chromium::DictionaryValue* schema,
                      const std::string& path);

  // Validates that the JSON node |instance| has |expected_type|.
  bool ValidateType(chromium::Value* instance, const std::string& expected_type,
                    const std::string& path);

  // Returns true if |schema| will allow additional items of any type.
  bool SchemaAllowsAnyAdditionalItems(
      chromium::DictionaryValue* schema,
      chromium::DictionaryValue** addition_items_schema);

  // The root schema node.
  chromium::DictionaryValue* schema_root_;

  // Map of user-defined name to type.
  TypeMap types_;

  // Whether we allow additional properties on objects by default. This can be
  // overridden by the allow_additional_properties flag on an Object schema.
  bool default_allow_additional_properties_;

  // Errors accumulated since the last call to Validate().
  std::vector<Error> errors_;
};

#endif  // CHROME_COMMON_JSON_SCHEMA_VALIDATOR_HH_
