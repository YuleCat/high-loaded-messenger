#pragma once

#include "..\lib\crow_all.h"
#include "utils.h"

#include <string>
#include <vector>

struct RequestCorrectnessInfo {
	bool isRequestCorrect;
	std::string username;
};

inline const RequestCorrectnessInfo GetRequestInfo(const crow::request& request) {
	std::vector<std::string> temp;
	boost::split(temp, request.body, boost::is_any_of("="));

	RequestCorrectnessInfo res;
	res.isRequestCorrect = temp[0] == "username";
	res.username = temp[1];
	return res;
}

inline std::string GetUserId(crow::App<crow::CookieParser>& app, const crow::request& request) {
	auto& context = app.get_context<crow::CookieParser>(request);
	return context.get_cookie("userId");
}

inline void LoadFriendsToJson(const std::vector<std::string>& friends, crow::json::wvalue& json) {
	for (int i = 0; i < friends.size(); ++i) {
		json["friends"][i] = friends[i];
	}
}

inline void LoadMessagesToJson(const std::vector<Message>& messages, crow::json::wvalue& json) {
	int messagesCount = 0;
	for (auto& item : messages) {
		json["messages"][messagesCount]["sender"] = item.sender;
		json["messages"][messagesCount]["message"] = item.message;
		json["messages"][messagesCount]["time_stamp"] = std::to_string(item.timeStamp);

		messagesCount++;
	}
}

inline void LoadSearchResultsToJson(const std::string& username, std::vector<std::string>& searchResults, crow::json::wvalue& json) {
	int counter = 0;
	for (auto& item : searchResults) {
		if (item != username) {
			json["search_results"][counter] = item;
			counter++;
		}
	}
}