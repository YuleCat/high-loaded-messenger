#include "..\lib\crow_all.h"
#include "utils.h"
#include <chrono>
#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

const std::string HTML_PAGES_PATH = "../../static/";

std::set<std::string> usernames;
std::unordered_map<std::string, std::set<std::string>> friends;
std::unordered_map<std::string,	std::unordered_map<std::string, std::vector<Message>>> database;
std::unordered_map<std::string, RequestInfo> wait_responses;

int main()
{
	srand(time(NULL));

	auto messages_numbers = std::vector<int>(100);
	for (int i = 0; i < 100; ++i) {
		if (i + 1 <= 25)
			messages_numbers[i] = 1;
		else if (i + 1 <= 40)
			messages_numbers[i] = 2;
		else if (i + 1 <= 50)
			messages_numbers[i] = 3;
		else if (i + 1 <= 70)
			messages_numbers[i] = 5;
		else if (i + 1 <= 90)
			messages_numbers[i] = 10;
		else
			messages_numbers[i] = 20;
	}

	std::mutex database_mutex;
	std::mutex res_mutex;
	std::mutex messages_mutex;

	crow::App<crow::CookieParser> app;
	crow::mustache::set_base(".");
	auto start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch())
		.count();

	for (int i = 1; i <= 10; ++i) {
		auto user_number = std::to_string(i);
		usernames.insert("User" + user_number);
		for (int j = 1; j <= 10; ++j) {
			if (i != j) {
				auto friend_number = std::to_string(j);
				friends["User" + user_number].insert("User" + friend_number);
			}
		}
	}

	CROW_ROUTE(app, "/").methods("GET"_method)([] {
		crow::mustache::context ctx;
		return crow::mustache::load(HTML_PAGES_PATH + "sign_in_page.html").render();
		});

	CROW_ROUTE(app, "/sign_in")
		.methods("POST"_method)([&app](const crow::request & req) {
		std::vector<std::string> username_info;
		if (!is_correct_username_request(req, username_info)) {
			return crow::response(400);
		}
		auto& context = app.get_context<crow::CookieParser>(req);
		context.set_cookie("username", username_info[1]);
		usernames.insert(username_info[1]);

		crow::response response;
		response.redirect("/chat");

		CROW_LOG_INFO << "user signed in: " << username_info[1];
		return response;
			});

	CROW_ROUTE(app, "/chat")
		.methods("GET"_method)([&app](const crow::request & req) {
		auto username = get_username(app, req);
		if (username == "") {
			return crow::response(401);
		}
		usernames.insert(username);

		crow::response response;
		response.body = crow::mustache::load(HTML_PAGES_PATH + "chat.html").render();
		load_friends(username, friends, response.json_value);

		return response;
			});

	CROW_ROUTE(app, "/chat/search/<string>")
		.methods("GET"_method)([&app](const crow::request & req, std::string query) {
		auto username = get_username(app, req);
		if (username == "") {
			return crow::response(401);
		}
		usernames.insert(username);

		crow::response response;

		find_users(query, username, usernames, response.json_value);

		return response;
			});

	CROW_ROUTE(app, "/chat/add_friend/<string>")
		.methods("POST"_method)(
			[&app](const crow::request & req, std::string friend_name) {
				auto username = get_username(app, req);
				if (username == "") {
					return crow::response(401);
				}
				usernames.insert(username);

				crow::response response;

				friends[username].insert(friend_name);
				friends[friend_name].insert(username);

				// load_friends(username, friends, response.json_value);

				return response;
			});

	CROW_ROUTE(app, "/chat/friends")
		.methods("GET"_method)([&app](const crow::request & req) {
		auto username = get_username(app, req);
		if (username == "") {
			return crow::response(401);
		}
		usernames.insert(username);

		crow::response response;

		load_friends(username, friends, response.json_value);

		return response;
			});

	CROW_ROUTE(app, "/chat/load_messages/<string>/<string>")
		.methods("GET"_method)([&app,
			&database_mutex,
			&res_mutex,
			&messages_mutex,
			&messages_numbers](const crow::request & req,
				crow::response & response,
				std::string friend_name,
				std::string str_time_stamp) {
					auto username = get_username(app, req);
					if (username == "") {
						response.code = 401;
						response.end();
						return;
					}
					usernames.insert(username);

					long long time_stamp = std::stoll(str_time_stamp);

					database_mutex.lock();
					auto messages_ptr = database[username].find(friend_name);
					if (messages_ptr != database[username].end()) {
						auto messages = messages_ptr->second;
						database_mutex.unlock();
						if (messages.back().time_stamp == time_stamp || messages.size() == 0) {
							/*res_mutex.lock();
							  wait_responses[username] = RequestInfo(&response,
							  std::chrono::steady_clock::now(), friend_name, time_stamp);
							  res_mutex.unlock();*/
										return;
						}

						auto possibility = rand() % 100;
						messages_mutex.lock();
						auto messages_number = messages_numbers[possibility];
						messages_mutex.unlock();

						for (int i = messages.size() - 1; i >= 0; --i) {
							if (messages[i].time_stamp > time_stamp) { // && messages_number > 0) {
								response.json_value["messages"][messages.size() - i - 1]["sender"] = messages[i].sender;
								response
									.json_value["messages"][messages.size() - i - 1]["message"]
									= messages[i].message;
								response
									.json_value["messages"][messages.size() - i - 1]["time_stamp"]
									= std::to_string(messages[i].time_stamp);

								messages_number--;
							}
						}
					}
					else {
						database_mutex.unlock();
						/*res_mutex.lock();
						wait_responses[username] = RequestInfo(&response,
						std::chrono::steady_clock::now(), friend_name, time_stamp);
						res_mutex.unlock();
						return;*/
					}

					response.code = 200;
					response.end();
					return;
			});

	CROW_ROUTE(app, "/chat/send")
		.methods("POST"_method)([&app, &start_time, &database_mutex, &res_mutex](
			const crow::request & req) {
				auto username = get_username(app, req);
				if (username == "") {
					return crow::response(401);
				}
				usernames.insert(username);

				crow::response response;

				auto json = crow::json::load(req.body);
				if (!json.has("message") || !json.has("friend_name")) {
					return crow::response(400);
				}

				std::string message = json["message"].s();
				std::string friend_name = json["friend_name"].s();
				auto time_stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count() - start_time;

				Message message_info(time_stamp, message, username);
				database_mutex.lock();
				database[username][friend_name].push_back(message_info);
				database[friend_name][username].push_back(message_info);
				database_mutex.unlock();

				/*res_mutex.lock();
				if (wait_responses.find(username) != wait_responses.end()) {
					if (wait_responses[username].friend_name == friend_name) {
						  wait_responses[username].response->json_value["messages"][0]["sender"] = username;
						  wait_responses[username].response->json_value["messages"][0]["time_stamp"] = std::to_string(time_stamp);
						  wait_responses[username].response->json_value["messages"][0]["message"] = message; wait_responses[username].response->end();
						  wait_responses.erase(username);
					}
				}

				if (wait_responses.find(friend_name) != wait_responses.end()) {
					if (wait_responses[friend_name].friend_name == username) {
						  wait_responses[friend_name].response->json_value["messages"][0]["sender"] = username;
						  wait_responses[friend_name].response->json_value["messages"][0]["time_stamp"] = std::to_string(time_stamp);
						  wait_responses[friend_name].response->json_value["messages"][0]["message"] = message; wait_responses[friend_name].response->end();
						  wait_responses.erase(friend_name);
					}
				}
				res_mutex.unlock();*/

				return response;
			});

	app.loglevel(crow::LogLevel::Info);

	app.port(18080).multithreaded().run();
}