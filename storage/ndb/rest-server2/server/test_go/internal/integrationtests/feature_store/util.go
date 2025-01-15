/*
 * This file is part of the RonDB REST API Server
 * Copyright (c) 2024 Hopsworks AB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
package feature_store

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"strings"
	"testing"

	"github.com/hamba/avro/v2"
)

func DeserialiseComplexFeature(t *testing.T, value *json.RawMessage, schema *avro.Schema) (*interface{}, error) {
	valueString, err := decodeJSONString(value)
	if err != nil {
		t.Fatalf("Failed to unmarshal. Value: %s", valueString)
		return nil, err
	}

	jsonDecode, err := base64.StdEncoding.DecodeString(valueString)
	if err != nil {
		t.Fatalf("Failed to decode base64. Value: %s", valueString)
		return nil, err
	}
	var avroDeserialized interface{}
	err = avro.Unmarshal(*schema, jsonDecode, &avroDeserialized)
	if err != nil {
		t.Fatalf("Failed to deserialize avro")
		return nil, err
	}

	nativeJson := ConvertAvroToJson(avroDeserialized)
	return &nativeJson, err
}

func decodeJSONString(raw *json.RawMessage) (string, error) {
	// Convert the raw message to a string
	rawStr := string(*raw)
	// Check that the first and last characters are quotes
	if len(rawStr) < 2 || rawStr[0] != '"' || rawStr[len(rawStr)-1] != '"' {
		return "", fmt.Errorf("invalid JSON string format")
	}
	// Remove the surrounding quotes
	unquotedStr := rawStr[1 : len(rawStr)-1]
	// Replace escape sequences with their actual characters
	decodedStr := strings.ReplaceAll(unquotedStr, `\"`, `"`)
	return decodedStr, nil
}

func ConvertAvroToJson(o interface{}) interface{} {
	var out interface{}
	switch o.(type) {
	case map[string]interface{}: // union or map
		{
			m := o.(map[string]interface{})
			for key := range m {
				switch m[key].(type) {
				case map[string]interface{}:
					{
						result := make(map[string]interface{})
						structValue := m[key].(map[string]interface{})
						for structKey := range structValue {
							result[structKey] = ConvertAvroToJson(structValue[structKey])
						}
						out = result
					}
				case []interface{}:
					{
						result := make([]interface{}, 0)
						for _, item := range m[key].([]interface{}) {
							itemJson := ConvertAvroToJson(item)
							result = append(result, itemJson)
						}
						out = result
					}
				default:
					{
						out = ConvertAvroToJson(m[key])
					}
				}
			}
		}
	case []interface{}:
		{
			result := make([]interface{}, 0)
			for _, item := range o.([]interface{}) {
				itemJson := ConvertAvroToJson(item)
				result = append(result, itemJson)
			}
			out = result
		}
	default:
		{
			out = o
		}
	}
	return out
}
