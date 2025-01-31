/*
 * This file is part of the RonDB REST API Server
 * Copyright (c) 2023 Hopsworks AB
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
	"encoding/json"
	"errors"
	"fmt"
	"sort"
	"strings"
	"time"

	"github.com/hamba/avro/v2"
	"github.com/patrickmn/go-cache"

	"hopsworks.ai/rdrs/internal/dal"
	"hopsworks.ai/rdrs/internal/log"
)

var DefaultExpiration time.Duration = 15 * time.Minute
var CleanupInterval time.Duration = 15 * time.Minute

const ERROR_NOT_FOUND = "Not Found"

type FeatureViewMetadata struct {
	FeatureStoreName   string
	FeatureStoreId     int
	FeatureViewName    string
	FeatureViewId      int
	FeatureViewVersion int
	// Feature view can be created with multiple features of the same name without prefix.
	// Each prefix column can map to multiple features
	PrefixFeaturesLookup map[string][]*FeatureMetadata // key: prefix + fName, label are excluded
	FeatureGroupFeatures []*FeatureGroupFeatures       // label are excluded
	FeatureStoreNames    []*string                     // List of all feature store used by feature view including shared feature store
	NumOfFeatures        int
	FeatureIndexLookup   map[string]int // key: joinIndex + fgId + fName, label are excluded. joinIndex is needed because of self-join
	// serving key doc: https://hopsworks.atlassian.net/wiki/spaces/FST/pages/173342721/How+to+resolve+the+set+of+serving+key+in+get+feature+vector
	PrimaryKeyMap      map[string]*dal.ServingKey // key: join index + feature name. Used for constructing rondb request.
	ValidPrimaryKeys   map[string]bool            // key: serving-key-prefix + fName, fName. Used for pk validation.
	PrefixJoinKeyMap   map[string][]string        // key: serving-key-prefix + fName, value: list of feature which join on the key. Used for filling in pk value.
	JoinKeyMap         map[string][]string        // key: fName, value: list of feature which join on the key. Used for filling in pk value.
	RequiredJoinKeyMap map[string][]string        // key: serving-key-prefix + fName, value: list of feature which join on the key. Used for filling in pk value.
	ComplexFeatures    map[string]*avro.Schema    // key: joinIndex + fgId + fName, label are excluded. joinIndex is needed because of self-join
}

type FeatureGroupFeatures struct {
	FeatureStoreName    string
	FeatureStoreId      int
	FeatureGroupName    string
	FeatureGroupVersion int
	FeatureGroupId      int
	JoinIndex           int
	Features            []*FeatureMetadata
	PrimaryKeyMap       []*dal.ServingKey
}

func (fgf *FeatureGroupFeatures) String() string {
	string1 := fmt.Sprintf("FeatureStoreName: %s, FeatureStoreId: %d, FeatureGroupName: %s, FeatureGroupVersion: %d, FeatureGroupId: %d",
		fgf.FeatureStoreName, fgf.FeatureStoreId, fgf.FeatureGroupName, fgf.FeatureGroupVersion, fgf.FeatureGroupId)
	string2 := fmt.Sprintf("JoinIndex: %d", fgf.JoinIndex)
	string3 := fmt.Sprintf("Features: %v", fgf.Features)
	string4 := fmt.Sprintf("PrimaryKeyMap: %v", fgf.PrimaryKeyMap)
	return fmt.Sprintf("%s\n%s\n%s\n%s", string1, string2, string3, string4)
}

type FeatureMetadata struct {
	FeatureStoreName    string
	FeatureGroupName    string
	FeatureGroupVersion int
	FeatureGroupId      int
	Id                  int
	Name                string
	Type                string
	Index               int
	Label               bool
	Prefix              string
	JoinIndex           int
}

var COMPLEX_FEATURE = map[string]bool{
	"MAP":       true,
	"ARRAY":     true,
	"STRUCT":    true,
	"UNIONTYPE": true,
}

func (f *FeatureMetadata) IsComplex() bool {
	return COMPLEX_FEATURE[strings.ToUpper(strings.Split(f.Type, "<")[0])]
}

func newFeatureViewMetadata(
	featureStoreName string,
	featureStoreId int,
	featureViewName string,
	featureViewId int,
	featureViewVersion int,
	features *[]*FeatureMetadata,
	servingKeys *[]dal.ServingKey,
) (*FeatureViewMetadata, error) {
	var ValidPrimaryKeysMap = make(map[string]bool)
	var primaryKeyMap = make(map[string]*dal.ServingKey)
	var fgPrimaryKeyMap = make(map[string][]*dal.ServingKey)
	var prefixJoinKeyMap = make(map[string][]string)
	var joinKeyMap = make(map[string][]string)
	var requiredJoinKeyMap = make(map[string][]string)
	var servingKeyMap = make(map[string]dal.ServingKey) // key: serving key prefix + fname

	for _, key := range *servingKeys {
		servingKeyMap[key.Prefix+key.FeatureName] = key
	}

	for _, key := range *servingKeys {
		var prefixFeatureName = key.Prefix + key.FeatureName
		ValidPrimaryKeysMap[prefixFeatureName] = true
		ValidPrimaryKeysMap[key.FeatureName] = true

		prefixJoinKeyMap[prefixFeatureName] = append(prefixJoinKeyMap[prefixFeatureName], prefixFeatureName)
		joinKeyMap[key.FeatureName] = append(joinKeyMap[key.FeatureName], prefixFeatureName)

		if key.Required {
			requiredJoinKeyMap[prefixFeatureName] = append(requiredJoinKeyMap[prefixFeatureName], prefixFeatureName)
		} else {
			if key.RequiredEntry != "" {
				requiredJoinKeyMap[key.JoinOn] = append(requiredJoinKeyMap[key.JoinOn], prefixFeatureName)
			}
		}
		var newKey = key
		primaryKeyMap[GetServingKey(key.JoinIndex, key.FeatureName)] = &newKey
		var fgKey = getFeatureGroupServingKey(key.JoinIndex, key.FeatureGroupId)
		fgPrimaryKeyMap[fgKey] = append(fgPrimaryKeyMap[fgKey], &newKey)
	}

	prefixColumns := make(map[string][]*FeatureMetadata)
	fgFeatures := make(map[string][]*FeatureMetadata)
	for _, feature := range *features {
		if feature.Label {
			continue
		}
		prefixFeatureName := feature.Prefix + feature.Name
		prefixColumns[prefixFeatureName] = append(prefixColumns[prefixFeatureName], feature)
		var featureKey = fmt.Sprintf("%d|%d", feature.JoinIndex, feature.FeatureGroupId)
		fgFeatures[featureKey] = append(fgFeatures[featureKey], feature)
	}

	var fgFeaturesArray = make([]*FeatureGroupFeatures, 0)

	for _, value := range fgFeatures {
		var featureValue = value
		var feature = featureValue[0]
		var fgFeature = FeatureGroupFeatures{}
		fgFeature.FeatureStoreName = feature.FeatureStoreName
		fgFeature.FeatureStoreId = featureStoreId
		fgFeature.FeatureGroupName = feature.FeatureGroupName
		fgFeature.FeatureGroupVersion = feature.FeatureGroupVersion
		fgFeature.FeatureGroupId = feature.FeatureGroupId
		fgFeature.Features = featureValue
		var fgPk, ok = fgPrimaryKeyMap[getFeatureGroupServingKey(feature.JoinIndex, feature.FeatureGroupId)]
		if ok {
			fgFeature.PrimaryKeyMap = fgPk
		}
		fgFeature.JoinIndex = feature.JoinIndex
		fgFeaturesArray = append(fgFeaturesArray, &fgFeature)
	}
	less := func(i, j int) bool {
		return (*features)[i].Index < (*features)[j].Index
	}
	sort.Slice(*features, less)
	featureIndex := make(map[string]int)

	var featureCount = 0
	for _, feature := range *features {
		if feature.Label {
			continue
		}
		featureIndexKey := GetFeatureIndexKeyByFeature(feature)
		featureIndex[featureIndexKey] = featureCount
		featureCount++
	}

	var complexFeatures = make(map[string]*avro.Schema)
	var fgSchemaCache = make(map[int]*dal.FeatureGroupAvroSchema)
	for _, fgFeature := range fgFeaturesArray {
		for _, feature := range fgFeature.Features {
			if (*feature).IsComplex() {
				if _, exist := fgSchemaCache[feature.FeatureGroupId]; !exist {
					projectId, dalErr := dal.GetProjectID(fgFeature.FeatureStoreName)
					if dalErr != nil {
						return nil, errors.New("Failed to get project id. " + dalErr.Error())
					}
					log.Debugf("project id is %d", projectId)
					newFgSchema, err := dal.GetFeatureGroupAvroSchema(
						fgFeature.FeatureGroupName,
						fgFeature.FeatureGroupVersion,
						projectId,
					)
					if err != nil {
						return nil, errors.New("Failed to get feature schema. " + err.Error())
					}
					fgSchemaCache[feature.FeatureGroupId] = newFgSchema
				}
				schemaStr, err := fgSchemaCache[feature.FeatureGroupId].GetSchemaByFeatureName(feature.Name)
				if err != nil {
					return nil, errors.New("Failed to get feature schema for feature: " + feature.Name)
				}
				schema, err := avro.Parse(string(schemaStr))
				if err != nil {
					return nil, errors.New("Failed to parse feature schema.")
				}
				featureIndexKey := GetFeatureIndexKeyByFeature(feature)
				complexFeatures[featureIndexKey] = &schema
			}
		}

	}
	var fsNames = []*string{}
	var fsNameMap = make(map[string]bool)
	for _, fgf := range fgFeaturesArray {
		var fgName = fgf.FeatureStoreName
		if !fsNameMap[fgName] {
			fsNames = append(fsNames, &fgName)
			fsNameMap[fgName] = true
		}
	}
	if !fsNameMap[featureStoreName] {
		fsNames = append(fsNames, &featureStoreName)
	}
	var numOfFeature = len(featureIndex)
	var metadata = FeatureViewMetadata{}
	metadata.FeatureStoreName = featureStoreName
	metadata.FeatureStoreId = featureStoreId
	metadata.FeatureViewName = featureViewName
	metadata.FeatureViewId = featureViewId
	metadata.FeatureViewVersion = featureViewVersion
	metadata.PrefixFeaturesLookup = prefixColumns
	metadata.FeatureGroupFeatures = fgFeaturesArray
	metadata.NumOfFeatures = numOfFeature
	metadata.FeatureIndexLookup = featureIndex
	metadata.FeatureStoreNames = fsNames
	metadata.PrimaryKeyMap = primaryKeyMap
	metadata.ValidPrimaryKeys = ValidPrimaryKeysMap
	metadata.PrefixJoinKeyMap = prefixJoinKeyMap
	metadata.RequiredJoinKeyMap = requiredJoinKeyMap
	metadata.JoinKeyMap = joinKeyMap
	metadata.ComplexFeatures = complexFeatures
	return &metadata, nil
}

func getFeatureGroupServingKey(joinIndex int, featureGroupId int) string {
	return fmt.Sprintf("%d|%d", joinIndex, featureGroupId)
}

func GetServingKey(joinIndex int, featureName string) string {
	return fmt.Sprintf("%d|%s", joinIndex, featureName)
}

func GetFeatureGroupKeyByFeature(feature *FeatureMetadata) string {
	return *getFeatureGroupIndexKey(feature.JoinIndex, feature.FeatureGroupId)
}

func GetFeatureGroupKeyByTDFeature(feature *FeatureGroupFeatures) string {
	return *getFeatureGroupIndexKey(feature.JoinIndex, feature.FeatureGroupId)
}

func GetFeatureIndexKeyByFeature(feature *FeatureMetadata) string {
	return *getFeatureIndexKey(feature.JoinIndex, feature.FeatureGroupId, feature.Name)
}

func GetFeatureIndexKeyByFgIndexKey(fgKey string, featureName string) string {
	return fmt.Sprintf("%s|%s", fgKey, featureName)
}

func getFeatureGroupIndexKey(joinIndex int, fgId int) *string {
	var indexKey = fmt.Sprintf("%d|%d", joinIndex, fgId)
	return &indexKey
}

func getFeatureIndexKey(joinIndex int, fgId int, f string) *string {
	var featureIndexKey = GetFeatureIndexKeyByFgIndexKey(*getFeatureGroupIndexKey(joinIndex, fgId), f)
	return &featureIndexKey
}

type FeatureViewMetaDataCache struct {
	metadataCache cache.Cache
}

func NewFeatureViewMetaDataCache() *FeatureViewMetaDataCache {
	var c = cache.New(DefaultExpiration, CleanupInterval)
	return &FeatureViewMetaDataCache{*c}
}

func (fvmeta *FeatureViewMetaDataCache) Get(featureStoreName, featureViewName string, featureViewVersion int) (*FeatureViewMetadata, *RestErrorCode) {
	var fvCacheKey = getFeatureViewCacheKey(featureStoreName, featureViewName, featureViewVersion)
	var metadataInf, exist = fvmeta.metadataCache.Get(fvCacheKey)
	if !exist {
		var metadata, err = GetFeatureViewMetadata(featureStoreName, featureViewName, featureViewVersion)
		if err != nil {
			return nil, err
		} else {
			fvmeta.metadataCache.SetDefault(fvCacheKey, metadata)
			if log.IsDebug() {
				metadataJson, _ := json.MarshalIndent(metadata, "", "  ")
				log.Debugf("Feature store metadata is %s", metadataJson)
			}
			return metadata, nil
		}
	} else {
		var metadata, ok = metadataInf.(*FeatureViewMetadata)
		if !ok {
			return nil, FETCH_METADATA_FROM_CACHE_FAIL
		}
		return metadata, nil
	}
}

func getFeatureViewCacheKey(featureStoreName, featureViewName string, featureViewVersion int) string {
	return fmt.Sprintf("%s|%s|%d", featureStoreName, featureViewName, featureViewVersion)
}

func GetFeatureViewMetadata(featureStoreName, featureViewName string, featureViewVersion int) (*FeatureViewMetadata, *RestErrorCode) {
	fsID, err := dal.GetFeatureStoreID(featureStoreName)
	if err != nil {
		if strings.Contains(err.Error(), ERROR_NOT_FOUND) {
			return nil, FS_NOT_EXIST
		}
		return nil, FS_NOT_EXIST.NewMessage(err.Error())
	}

	fvID, err := dal.GetFeatureViewID(fsID, featureViewName, featureViewVersion)
	if err != nil {
		if strings.Contains(err.Error(), ERROR_NOT_FOUND) {
			return nil, FV_NOT_EXIST
		}
		return nil, FV_READ_FAIL.NewMessage(err.VerboseError())
	}

	joinIdToJoin := make(map[int]dal.TrainingDatasetJoin)
	tdJoins, err := dal.GetTrainingDatasetJoinData(fvID)
	if err != nil {
		if strings.Contains(err.Error(), ERROR_NOT_FOUND) {
			return nil, FG_NOT_EXIST.NewMessage("Feature view may contain deleted feature groups.")
		}
		return nil, TD_JOIN_READ_FAIL.NewMessage(err.VerboseError())
	}
	for _, tdj := range tdJoins {
		joinIdToJoin[tdj.Id] = tdj
	}

	tdfs, err := dal.GetTrainingDatasetFeature(fvID)
	if err != nil {
		return nil, TD_FEATURE_READ_FAIL.NewMessage(err.VerboseError())
	}

	features := make([]*FeatureMetadata, len(tdfs))
	fsIdToName := make(map[int]string)
	var fgCache = make(map[int]*dal.FeatureGroup)

	for i, tdf := range tdfs {
		if tdf.FeatureGroupID == 0 {
			// If a feature group is deleted, feature group id will be set to null in the db.
			return nil, FG_NOT_EXIST.NewMessage(
				fmt.Sprintf("Cannot get the feature group of feature `%s`. Check if the feature group still exists.", tdf.Name))
		}
		var featureGroup *dal.FeatureGroup
		if fg, ok := fgCache[tdf.FeatureGroupID]; ok {
			featureGroup = fg
		} else {
			featureGroup, err = dal.GetFeatureGroupData(tdf.FeatureGroupID)
			if err != nil {
				if strings.Contains(err.Error(), ERROR_NOT_FOUND) {
					return nil, FG_NOT_EXIST
				}
				return nil, FG_READ_FAIL.NewMessage(err.VerboseError())
			}
			fgCache[tdf.FeatureGroupID] = featureGroup
		}
		feature := FeatureMetadata{}
		if featureStoreName, exist := fsIdToName[featureGroup.FeatureStoreId]; exist {
			feature.FeatureStoreName = featureStoreName
		} else {
			featureStoreName, err = dal.GetFeatureStoreName(featureGroup.FeatureStoreId)
			if err != nil {
				if strings.Contains(err.Error(), ERROR_NOT_FOUND) {
					return nil, FS_NOT_EXIST
				}
				return nil, FS_READ_FAIL.NewMessage(err.VerboseError())
			}
			fsIdToName[featureGroup.FeatureStoreId] = featureStoreName
			feature.FeatureStoreName = featureStoreName
		}
		feature.FeatureGroupName = featureGroup.Name
		feature.FeatureGroupVersion = featureGroup.Version
		feature.FeatureGroupId = tdf.FeatureGroupID
		feature.Id = tdf.FeatureID
		feature.Name = tdf.Name
		feature.Type = tdf.Type
		feature.Index = tdf.IDX
		feature.Label = tdf.Label == 1
		feature.Prefix = joinIdToJoin[tdf.TDJoinID].Prefix
		feature.JoinIndex = joinIdToJoin[tdf.TDJoinID].Index
		features[i] = &feature
	}
	var servingKeys, err1 = dal.GetServingKeys(fvID)
	if err1 != nil {
		return nil, FV_READ_FAIL.NewMessage("Failed to read serving keys.")
	}
	featureViewMetadata, err2 := newFeatureViewMetadata(
		featureStoreName,
		fsID,
		featureViewName,
		fvID,
		featureViewVersion,
		&features,
		&servingKeys,
	)
	if err2 != nil {
		return nil, FV_READ_FAIL.NewMessage(err2.Error())
	}
	return featureViewMetadata, nil
}
