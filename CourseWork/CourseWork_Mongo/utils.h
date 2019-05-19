#pragma once

#include "..\lib\crow_all.h"

#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

struct Message {
	std::string sender;
	std::string message;
	long long timeStamp;

	Message(std::string sender, std::string message, long long timeStamp)
		: sender(sender)
		, message(message)
		, timeStamp(timeStamp)
	{
	}
};

struct RequestInfo {
	crow::response* response;
	decltype(std::chrono::steady_clock::now()) birth_time;
	bsoncxx::oid friend_name;
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

inline std::vector<int>
CreateMessagesNumbers()
{
	auto messagesCount = std::vector<int>(100);
	for (int i = 0; i < 100; ++i) {
		if (i + 1 <= 25)
			messagesCount[i] = 1;
		else if (i + 1 <= 40)
			messagesCount[i] = 2;
		else if (i + 1 <= 50)
			messagesCount[i] = 3;
		else if (i + 1 <= 70)
			messagesCount[i] = 5;
		else if (i + 1 <= 90)
			messagesCount[i] = 10;
		else
			messagesCount[i] = 20;
	}

	return messagesCount;
}
