#include "settingsReader.h"

namespace PrivateFunctions {
	bool ShouldRebuildINI(CSimpleIniA* a_ini) {
		const char* generalKeys[] = {
			"bSuppressOriginalShader",
			"bShouldAddLight" };

		std::list<CSimpleIniA::Entry> keyHolder;
		a_ini->GetAllKeys("General", keyHolder);
		if (std::size(generalKeys) != keyHolder.size()) return true;

		for (auto* key : generalKeys) {
			if (!a_ini->KeyExists("General", key)) return true;
		}
		return false;
	}
}

namespace Settings {
	bool BuildIni() {
		std::filesystem::path f{ "Data/SKSE/Plugins/EnchantmentArtExtender.ini" };
		bool createEntries = false;
		if (!std::filesystem::exists(f)) {
			std::fstream createdINI;
			createdINI.open(f, std::ios::out);
			createdINI.close();
			createEntries = true;
		}

		CSimpleIniA ini;
		ini.SetUnicode();
		ini.LoadFile(f.c_str());
		if (!createEntries) { createEntries = PrivateFunctions::ShouldRebuildINI(&ini); }

		if (createEntries) {
			ini.Delete("General", NULL);
			ini.SetBoolValue("General", "bSuppressOriginalShader", true, ";Disables the original shader of the enchantment. Recommended: false");
			ini.SetBoolValue("General", "bShouldAddLight", true, ";Mixes the light of the given enchantments (provided they have light). Recommended: true");
			ini.SaveFile(f.c_str());
		}

		return true;
	}

	ErrorReport Settings::IsConfigValid(std::string config, Json::Value a_JSON) {
		ErrorReport response = ErrorReport(config);
		const char* expectedInformationNames[] = { "Left", "Right" };
		const char* expectedInformationAtLeastOne[] = { "RequiredWeapons", "WeaponKeywords" };
		const char* otherValidFields[] = { "ExcludedWeapons", "ExcludedWeaponKeywords", "Exclusive", "$schema" };
		const char** allAllowedFields[] = { expectedInformationNames, expectedInformationAtLeastOne, otherValidFields };

		//Empty, un-openable configs are reported in the original reader.
		//Check 1: Check for outdated framework version.
		auto minVersionField = a_JSON["MinimumVersion"];
		int minVersionInt = minVersionField ? minVersionField.asInt() : 0;
		int currentVersion = Version::MAJOR + Version::MINOR + Version::PATCH;
		if (minVersionInt > currentVersion) {
			response.valid = false;
			response.outdatedFrameworkVersion = true;
			return response;
		}

		//Check 2: Check for missing fields.
		std::string requiredFields[] = { "EnchantmentKeywords", "ArtSource", "SwapData" };
		bool foundAllFields = true;

		for (auto field : requiredFields) {
			auto found = a_JSON[field];
			if (!found) {
				foundAllFields = false;
				response.valid = false;
				response.missingRequiredFields.push_back(field);
			}
		}

		//Check 3: Check for garbage.
		std::string expectedFields[] = { "MinimumVersion", "EnchantmentKeywords", "ArtSource", "SwapData", "Exclusive", "$schema" };
		auto allFields = a_JSON.getMemberNames();
		for (auto field : allFields) {
			bool fieldIsValid = false;
			for (auto expectedField : expectedFields) {
				if (field == expectedField) {
					fieldIsValid = true;
				}
			}

			if (!fieldIsValid) {
				response.valid = false;
				response.foundGarbage.push_back(field);
			}
		}

		//Check 4: Unexpected objects in the top level.
		std::string expectedString[] = { "ArtSource" };
		std::string expectedList[] = { "EnchantmentKeywords" , "SwapData" };
		std::string expectedBool[] = { "Exclusive" };

		for (auto entry : expectedString) {
			auto field = a_JSON[entry];
			if (!field) continue;
			if (field.isString()) continue;
			response.valid = false;
			response.expectedString.push_back(entry);
		}

		for (auto entry : expectedList) {
			auto field = a_JSON[entry];
			if (!field) continue;
			if (field.isArray()) continue;
			response.valid = false;
			response.expectedString.push_back(entry);
		}

		for (auto entry : expectedBool) {
			auto field = a_JSON[entry];
			if (!field) continue;
			if (field.isBool()) continue;
			response.valid = false;
			response.expectedBool.push_back(entry);
		}

		//Check for garbage and missing masters in SwapData.
		if (a_JSON["SwapData"] && a_JSON["SwapData"].isArray()) {
			for (auto data : a_JSON["SwapData"]) {
				if (!data.isObject()) {
					response.valid = false;
					response.expectedObject.push_back("thing");
					continue;
				}

				auto allDataNames = data.getMemberNames();
				if (allDataNames.size() != 1) {
					response.valid = false;
					response.badSwapData.push_back("Non-object in SwapData.");
					continue;
				}

				auto swapName = allDataNames.at(0);
				auto swapInformation = data[swapName];
				if (!swapInformation.isObject()) {
					response.valid = false;
					response.badSwapData.push_back("Expected " + swapName + " to be an object, but isn't.");
					continue;
				}

				auto informationNames = swapInformation.getMemberNames();
				int matches = 0;
				for (auto name : expectedInformationNames) {
					for (std::string field : informationNames) {
						if (field == std::string(name)) {
							++matches;
						}
					}
				}

				if (matches != sizeof(expectedInformationNames) / sizeof(*expectedInformationNames)) {
					response.valid = false;
					response.badSwapData.push_back(swapName + " is missing at least one necessary field.");
					continue;
				}

				matches = 0;
				for (auto name : informationNames) {
					for (auto field : expectedInformationAtLeastOne) {
						if (name == std::string(field)) {
							++matches;
						}
					}
				}

				if (matches < 1) {
					response.valid = false;
					response.badSwapData.push_back(swapName + " is missing at least one necessary field.");
					continue;
				}
			}
		}
		return response;
	}
}