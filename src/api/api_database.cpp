/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2013-2016    WrinklyNinja

    This file is part of LOOT.

    LOOT is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    LOOT is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with LOOT.  If not, see
    <https://www.gnu.org/licenses/>.
    */

#include "api/api_database.h"

#include <unordered_map>
#include <vector>

#include <boost/algorithm/string.hpp>

#include "api/game/game.h"
#include "api/metadata/condition_evaluator.h"
#include "api/metadata/yaml/plugin_metadata.h"
#include "api/plugin/plugin_sorter.h"
#include "loot/exception/file_access_error.h"

namespace loot {
ApiDatabase::ApiDatabase(const GameType gameType,
                         const boost::filesystem::path& dataPath,
                         std::shared_ptr<GameCache> gameCache,
                         std::shared_ptr<LoadOrderHandler> loadOrderHandler) :
    gameCache_(gameCache),
    conditionEvaluator_(gameType, dataPath, gameCache, loadOrderHandler) {}

///////////////////////////////////
// Database Loading Functions
///////////////////////////////////

void ApiDatabase::LoadLists(const std::string& masterlistPath,
                            const std::string& userlistPath) {
  Masterlist temp;
  MetadataList userTemp;

  if (!masterlistPath.empty()) {
    if (boost::filesystem::exists(masterlistPath)) {
      temp.Load(masterlistPath);
    } else {
      throw FileAccessError("The given masterlist path does not exist: " +
                            masterlistPath);
    }
  }

  if (!userlistPath.empty()) {
    if (boost::filesystem::exists(userlistPath)) {
      userTemp.Load(userlistPath);
    } else {
      throw FileAccessError("The given userlist path does not exist: " +
                            userlistPath);
    }
  }

  masterlist_ = temp;
  userlist_ = userTemp;
}

void ApiDatabase::WriteUserMetadata(const std::string& outputFile,
                                    const bool overwrite) const {
  if (!boost::filesystem::exists(
          boost::filesystem::path(outputFile).parent_path()))
    throw std::invalid_argument("Output directory does not exist.");

  if (boost::filesystem::exists(outputFile) && !overwrite)
    throw FileAccessError(
        "Output file exists but overwrite is not set to true.");

  userlist_.Save(outputFile);
}

////////////////////////////////////
// LOOT Functionality Functions
////////////////////////////////////

bool ApiDatabase::UpdateMasterlist(const std::string& masterlistPath,
                                   const std::string& remoteURL,
                                   const std::string& remoteBranch) {
  if (!boost::filesystem::is_directory(
          boost::filesystem::path(masterlistPath).parent_path()))
    throw std::invalid_argument("Given masterlist path \"" + masterlistPath +
                                "\" does not have a valid parent directory.");

  Masterlist masterlist;
  if (masterlist.Update(masterlistPath, remoteURL, remoteBranch)) {
    masterlist_ = masterlist;
    return true;
  }

  return false;
}

MasterlistInfo ApiDatabase::GetMasterlistRevision(
    const std::string& masterlistPath,
    const bool getShortID) const {
  return Masterlist::GetInfo(masterlistPath, getShortID);
}

bool ApiDatabase::IsLatestMasterlist(const std::string& masterlist_path,
                                     const std::string& branch) const {
  return Masterlist::IsLatest(masterlist_path, branch);
}

//////////////////////////
// DB Access Functions
//////////////////////////

std::set<std::string> ApiDatabase::GetKnownBashTags() const {
  auto masterlistTags = masterlist_.BashTags();
  auto userlistTags = userlist_.BashTags();

  if (!userlistTags.empty()) {
    masterlistTags.insert(std::begin(userlistTags), std::end(userlistTags));
  }

  return masterlistTags;
}

std::vector<Message> ApiDatabase::GetGeneralMessages(
    bool evaluateConditions) const {
  auto masterlistMessages = masterlist_.Messages();
  auto userlistMessages = userlist_.Messages();

  if (!userlistMessages.empty()) {
    masterlistMessages.insert(std::end(masterlistMessages),
                              std::begin(userlistMessages),
                              std::end(userlistMessages));
  }

  if (evaluateConditions) {
    // Evaluate conditions from scratch.
    gameCache_->ClearCachedConditions();
    for (auto it = std::begin(masterlistMessages);
         it != std::end(masterlistMessages);) {
      if (!conditionEvaluator_.evaluate(it->GetCondition()))
        it = masterlistMessages.erase(it);
      else
        ++it;
    }
  }

  return masterlistMessages;
}

PluginMetadata ApiDatabase::GetPluginMetadata(const std::string& plugin,
                                              bool includeUserMetadata,
                                              bool evaluateConditions) const {
  PluginMetadata metadata = masterlist_.FindPlugin(plugin);

  if (includeUserMetadata) {
    metadata.MergeMetadata(userlist_.FindPlugin(plugin));
  }

  if (evaluateConditions) {
    return conditionEvaluator_.evaluateAll(metadata);
  }

  return metadata;
}

PluginMetadata ApiDatabase::GetPluginUserMetadata(
    const std::string& plugin,
    bool evaluateConditions) const {
  PluginMetadata metadata = userlist_.FindPlugin(plugin);

  if (evaluateConditions) {
    return conditionEvaluator_.evaluateAll(metadata);
  }

  return metadata;
}

void ApiDatabase::SetPluginUserMetadata(const PluginMetadata& pluginMetadata) {
  userlist_.ErasePlugin(pluginMetadata);
  userlist_.AddPlugin(pluginMetadata);
}

void ApiDatabase::DiscardPluginUserMetadata(const std::string& plugin) {
  userlist_.ErasePlugin(plugin);
}

void ApiDatabase::DiscardAllUserMetadata() { userlist_.Clear(); }

// Writes a minimal masterlist that only contains mods that have Bash Tag
// suggestions, and/or dirty messages, plus the Tag suggestions and/or messages
// themselves and their conditions, in order to create the Wrye Bash taglist.
// outputFile is the path to use for output. If outputFile already exists, it
// will only be overwritten if overwrite is true.
void ApiDatabase::WriteMinimalList(const std::string& outputFile,
                                   const bool overwrite) const {
  if (!boost::filesystem::exists(
          boost::filesystem::path(outputFile).parent_path()))
    throw std::invalid_argument("Output directory does not exist.");

  if (boost::filesystem::exists(outputFile) && !overwrite)
    throw FileAccessError(
        "Output file exists but overwrite is not set to true.");

  MetadataList minimalList;
  for (const auto& plugin : masterlist_.Plugins()) {
    PluginMetadata minimalPlugin(plugin.GetName());
    minimalPlugin.SetTags(plugin.GetTags());
    minimalPlugin.SetDirtyInfo(plugin.GetDirtyInfo());

    minimalList.AddPlugin(minimalPlugin);
  }

  minimalList.Save(outputFile);
}
}
