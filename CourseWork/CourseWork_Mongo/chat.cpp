#include "..\lib\crow_all.h"
#include "constants.h"
#include "crow_utils.h"
#include "mongo_service.h"
#include "utils.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

int main() {
	srand(time(NULL));

	std::mutex resMutex, messagesMutex;
	auto messagesNumbers = CreateMessagesNumbers();
	auto startTime = std::chrono::system_clock::now();

	crow::App<crow::CookieParser> app;
	crow::mustache::set_base(".");
	std::map<bsoncxx::oid, RequestInfo> waitResponses;

	mongocxx::uri uri{ CHAT_DB_URI };
	auto instance = std::shared_ptr<mongocxx::instance>(new mongocxx::instance{});
	auto pool = std::shared_ptr<mongocxx::pool>(new mongocxx::pool{ uri });

	MongoService mongoService(instance, pool);

	if (IS_TESTING) {
		mongoService.TryAddUsers();
	}

	CROW_ROUTE(app, "/").methods("GET"_method)([] {
		auto signInPage = crow::mustache::load(HTML_PAGES_PATH + SIGN_IN_PAGE);
		return signInPage.render();
		});

	CROW_ROUTE(app, "/sign_in")
		.methods("POST"_method)([&](const crow::request & req) {
		auto requestCorrectnessInfo = GetRequestInfo(req);
		if (!requestCorrectnessInfo.isRequestCorrect) {
			return crow::response(400);
		}
		auto& context = app.get_context<crow::CookieParser>(req);

		mongoService.TryAddUser(requestCorrectnessInfo.username);
		auto dbUserId = mongoService.GetUserId(requestCorrectnessInfo.username);
		if (dbUserId == "-1") {
			return crow::response(401);
		}

		context.set_cookie("userId", dbUserId);

		crow::response response;
		response.redirect("/chat");

		CROW_LOG_INFO << "user signed in: " << requestCorrectnessInfo.username;
		return response;
			});

	CROW_ROUTE(app, "/chat").methods("GET"_method)([&](const crow::request & req) {
		auto userId = GetUserId(app, req);
		if (userId == "") {
			return crow::response(401);
		}

		if (!mongoService.IsUserAdded(bsoncxx::oid::oid(userId))) {
			return crow::response(401);
		}

		crow::response response;
		response.body = crow::mustache::load(HTML_PAGES_PATH + CHAT_PAGE).render();
		auto friends = mongoService.LoadFriends(bsoncxx::oid::oid(userId));
		LoadFriendsToJson(friends, response.json_value);

		return response;
		});

	CROW_ROUTE(app, "/chat/search/<string>")
		.methods("GET"_method)([&](const crow::request & req, std::string query) {
		auto userId = GetUserId(app, req);
		if (userId == "") {
			return crow::response(401);
		}

		if (!mongoService.IsUserAdded(bsoncxx::oid::oid(userId))) {
			return crow::response(401);
		}

		crow::response response;

		auto username = mongoService.GetUsername(bsoncxx::oid::oid(userId));
		auto searchResults = mongoService.FindUsers(query);
		LoadSearchResultsToJson(username, searchResults, response.json_value);

		return response;
			});

	CROW_ROUTE(app, "/chat/add_friend/<string>")
		.methods("POST"_method)(
			[&](const crow::request & req, std::string friendName) {
				auto userId = GetUserId(app, req);
				if (userId == "") {
					return crow::response(401);
				}

				if (!mongoService.IsUserAdded(bsoncxx::oid::oid(userId))) {
					return crow::response(401);
				}

				auto friendId = mongoService.GetUserId(friendName);

				crow::response response;
				mongoService.MakeFriends(bsoncxx::oid::oid(userId),
					bsoncxx::oid::oid(friendId));

				return response;
			});

	CROW_ROUTE(app, "/chat/friends")
		.methods("GET"_method)([&](const crow::request & req) {
		auto userId = GetUserId(app, req);
		if (userId == "") {
			return crow::response(401);
		}

		if (!mongoService.IsUserAdded(bsoncxx::oid::oid(userId))) {
			return crow::response(401);
		}

		crow::response response;

		auto friends = mongoService.LoadFriends(bsoncxx::oid::oid(userId));
		LoadFriendsToJson(friends, response.json_value);

		return response;
			});

	CROW_ROUTE(app, "/chat/load_messages/<string>/<string>")
		.methods(
			"GET"_method)([&](const crow::request & req, crow::response & response,
				std::string friendName, std::string strTimeStamp) {
					auto userId = GetUserId(app, req);
					if (userId == "") {
						response.code = 401;
						response.end();
						return;
					}

					if (!mongoService.IsUserAdded(bsoncxx::oid::oid(userId))) {
						response.code = 401;
						response.end();
						return;
					}

					if (strTimeStamp == "0") {
						strTimeStamp = "0000000000000000";
					}
					auto timeStamp = bsoncxx::types::b_date(
						std::chrono::milliseconds(std::stoll(strTimeStamp)));

					int messagesNumber = 10;
					if (IS_TESTING) {
						messagesMutex.lock();
						messagesNumber = messagesNumbers[rand() % 100];
						messagesMutex.unlock();
					}

					auto friendId = mongoService.GetUserId(friendName);
					auto messages = mongoService.LoadMessages(bsoncxx::oid::oid(userId),
						bsoncxx::oid::oid(friendId),
						timeStamp, messagesNumber);

					if (messages.size() == 0 && !IS_TESTING) {
						resMutex.lock();
						waitResponses[bsoncxx::oid::oid(userId)] =
							RequestInfo(&response, std::chrono::steady_clock::now(),
								friendName, timeStamp.to_int64());
						resMutex.unlock();
						return;
					}

					LoadMessagesToJson(messages, response.json_value);

					response.code = 200;
					response.end();
					return;
				});

	CROW_ROUTE(app, "/chat/send")
		.methods("POST"_method)([&](const crow::request & req) {
		auto userId = GetUserId(app, req);
		if (userId == "") {
			return crow::response(401);
		}

		if (!mongoService.IsUserAdded(bsoncxx::oid::oid(userId))) {
			return crow::response(401);
		}

		crow::response response;

		auto json = crow::json::load(req.body);
		if (!json.has("message") || !json.has("friend_name")) {
			return crow::response(400);
		}

		auto currentTime = std::chrono::system_clock::now() - startTime;
		auto timeStamp = bsoncxx::types::b_date{
			std::chrono::duration_cast<std::chrono::milliseconds>(currentTime) };
		std::string message = json["message"].s();
		std::string friendName = json["friend_name"].s();

		auto friendId = mongoService.GetUserId(friendName);
		mongoService.InsertMessage(bsoncxx::oid::oid(userId),
			bsoncxx::oid::oid(friendId), message,
			timeStamp);

		if (!IS_TESTING) {
			auto oidUserId = bsoncxx::oid::oid(userId);
			auto oidFriendId = bsoncxx::oid::oid(friendId);

			resMutex.lock();
			if (waitResponses.find(oidUserId) != waitResponses.end()) {
				if (waitResponses[oidUserId].friend_name == oidFriendId) {
					waitResponses[oidUserId]
						.response->json_value["messages"][0]["sender"] =
						mongoService.GetUsername(oidUserId);
					waitResponses[oidUserId]
						.response->json_value["messages"][0]["time_stamp"] =
						std::to_string(timeStamp);
					waitResponses[oidUserId]
						.response->json_value["messages"][0]["message"] = message;
					waitResponses[oidUserId].response->end();
					waitResponses.erase(oidUserId);
				}
			}

			if (waitResponses.find(oidFriendId) != waitResponses.end()) {
				if (waitResponses[oidFriendId].friend_name == oidUserId) {
					waitResponses[oidFriendId]
						.response->json_value["messages"][0]["sender"] =
						mongoService.GetUsername(oidUserId);
					waitResponses[oidFriendId]
						.response->json_value["messages"][0]["time_stamp"] =
						std::to_string(timeStamp);
					waitResponses[oidFriendId]
						.response->json_value["messages"][0]["message"] = message;
					waitResponses[oidFriendId].response->end();
					waitResponses.erase(oidFriendId);
				}
			}
			resMutex.unlock();
		}

		return response;
			});

	if (IS_TESTING) {
		app.loglevel(crow::LogLevel::Warning);
	}
	else {
		app.loglevel(crow::LogLevel::Info);
	}

	app.port(CROW_PORT).multithreaded().run();
}
