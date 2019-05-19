#pragma once

#include "..\lib\crow_all.h"
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

struct Message {
	long long time_stamp;
	std::string message;
	std::string sender;

	Message(long long time_stamp, std::string message, std::string sender)
		: time_stamp(time_stamp)
		, message(message)
		, sender(sender)
	{
	}
};

struct RequestInfo {
	crow::response* response;
	decltype(std::chrono::steady_clock::now()) birth_time;
	std::string friend_name;
	long long time_stamp;

	RequestInfo() = default;

	RequestInfo(crow::response* res,
		decltype(std::chrono::steady_clock::now()) birth_time,
		std::string friend_name,
		long long time_stamp)
		: response(res)
		, birth_time(birth_time)
		, friend_name(friend_name)
		, time_stamp(time_stamp)
	{
	}
};

inline const bool
is_correct_username_request(const crow::request& request,
	std::vector<std::string>& username_info)
{
	boost::split(username_info, request.body, boost::is_any_of("="));
	return username_info[0] == "username";
}

inline const std::string
get_username(crow::App<crow::CookieParser>& app, const crow::request& request)
{
	auto& context = app.get_context<crow::CookieParser>(request);
	return context.get_cookie("username");
}

inline void
load_friends(const std::string& username,
	std::unordered_map<std::string, std::set<std::string>>& friends,
	crow::json::wvalue& json)
{
	int friends_count = 0;
	for (auto friend_name : friends[username]) {
		json["friends"][friends_count++] = friend_name;
	}
}

inline void
find_users(const std::string& query,
	const std::string& username,
	std::set<std::string>& usernames,
	crow::json::wvalue& json)
{
	auto lower_bound = usernames.lower_bound(query);
	int search_results_count = 0;
	for (auto it = lower_bound;
		it != usernames.end() && search_results_count < 100;
		++it) {
		if (*it == username) {
			continue;
		}

		if ((*it).substr(0, query.size()) == query) {
			json["search_results"][search_results_count++] = *it;
		}
		else {
			break;
		}
	}
}

inline void
load_messages(
	const std::string& username,
	const crow::request& request,
	std::unordered_map<std::string,
	std::unordered_map<std::string, std::vector<Message>>>&
	data_base,
	crow::json::wvalue& json)
{
	std::string friend_name = request.url_params.get("friend_name");
	long long time_stamp = std::stoll(request.url_params.get("time_stamp"));

	if (data_base[username].find(friend_name) != data_base[username].end()) {
		auto messages = data_base[username][friend_name];

		if (messages.size() == 0) {
			json["time_stamp"] = "0";
			return;
		}

		for (int i = messages.size() - 1; i >= 0; --i) {
			if (messages[i].time_stamp > time_stamp) {
				json["messages"][messages.size() - i - 1]["sender"] = messages[i].sender;
				json["messages"][messages.size() - i - 1]["message"] = messages[i].message;
				json["messages"][messages.size() - i - 1]["time_stamp"] = std::to_string(messages[i].time_stamp);
			}
		}
	}
}
