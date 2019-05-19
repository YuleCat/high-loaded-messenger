//////	#include "../lib/crow_all.h"
//////
//////int main() {
//////	crow::SimpleApp app;
//////
//////	CROW_ROUTE(app, "/")
//////		([]() {
//////		return "Hello world!";
//////	});
//////
//////	app.port(18080).run();
//////
//////	return 0;
//////}
//#include "..\lib\crow_all.h"
//#include <string>
//#include <vector>
//#include <chrono>
//
// using namespace std;
//
// vector<string> msgs;
// vector<pair<crow::response*, decltype(chrono::steady_clock::now())>> ress;
//
// void broadcast(const string& msg)
//{
//	msgs.push_back(msg);
//	crow::json::wvalue x;
//	x["msgs"][0] = msgs.back();
//	x["last"] = msgs.size();
//	string body = crow::json::dump(x);
//	for (auto p : ress)
//	{
//		auto* res = p.first;
//		CROW_LOG_DEBUG << res << " replied: " << body;
//		res->end(body);
//	}
//	ress.clear();
//}
// To see how it works go on {ip}:40080 but I just got it working with external
// build (not directly in IDE, I guess a problem with dependency)
// int main()
//{
//	crow::SimpleApp app;
//	crow::mustache::set_base(".");
//
//	CROW_ROUTE(app, "/")
//		([] {
//		crow::mustache::context ctx;
//		return
// crow::mustache::load("../static/example_chat.html").render();
//	});
//
//	CROW_ROUTE(app, "/logs")
//		([] {
//		CROW_LOG_INFO << "logs requested";
//		crow::json::wvalue x;
//		int start = max(0, (int)msgs.size() - 100);
//		for (int i = start; i < (int)msgs.size(); i++)
//			x["msgs"][i - start] = msgs[i];
//		x["last"] = msgs.size();
//		CROW_LOG_INFO << "logs completed";
//		return x;
//	});
//
//	CROW_ROUTE(app, "/logs/<int>")
//		([](const crow::request& /*req*/, crow::response& res, int
// after) { 		CROW_LOG_INFO << "logs with last " << after;
// if (after < (int)msgs.size())
//		{
//			crow::json::wvalue x;
//			for (int i = after; i < (int)msgs.size(); i++)
//				x["msgs"][i - after] = msgs[i];
//			x["last"] = msgs.size();
//
//			res.write(crow::json::dump(x));
//			res.end();
//		}
//		else
//		{
//			vector<pair<crow::response*,
// decltype(chrono::steady_clock::now())>> filtered; 			for
// (auto p : ress)
//			{
//				if (p.first->is_alive() &&
// chrono::steady_clock::now()
//- p.second < chrono::seconds(30))
// filtered.push_back(p); 				else
// p.first->end();
//			}
//			ress.swap(filtered);
//			ress.push_back({ &res, chrono::steady_clock::now() });
//			CROW_LOG_DEBUG << &res << " stored " << ress.size();
//		}
//	});
//
//	CROW_ROUTE(app, "/send")
//		.methods("GET"_method, "POST"_method)
//		([](const crow::request& req)
//	{
//		CROW_LOG_INFO << "msg from client: " << req.body;
//		broadcast(req.body);
//		return "";
//	});
//
//	app.port(40080)
//		.multithreaded()
//		.run();
//}
//
//#include "../lib/crow_all.h"
//#include <unordered_set>
//#include <mutex>
//
//
// int main()
//{
//	crow::SimpleApp app;
//	crow::mustache::set_base(".");
//
//	std::mutex mtx;
//	std::unordered_set<crow::websocket::connection*> users;
//
//	CROW_ROUTE(app, "/ws")
//		.websocket()
//		.onopen([&](crow::websocket::connection& conn) {
//		CROW_LOG_INFO << "new websocket connection";
//		std::lock_guard<std::mutex> _(mtx);
//		users.insert(&conn);
//	})
//		.onclose([&](crow::websocket::connection& conn, const
// std::string&
// reason) { 		CROW_LOG_INFO << "websocket connection closed: " <<
// reason; 		std::lock_guard<std::mutex> _(mtx);
// users.erase(&conn);
//	})
//		.onmessage([&](crow::websocket::connection& /*conn*/, const
// std::string& data, bool is_binary) {
// std::lock_guard<std::mutex>
// _(mtx); 		for (auto u : users) 			if (is_binary)
// u->send_binary(data); 			else
// u->send_text(data);
//	});
//
//	CROW_ROUTE(app, "/")
//		([] {
//		char name[256];
//		gethostname(name, 256);
//		crow::mustache::context x;
//		x["servername"] = name;
//
//		auto page = crow::mustache::load("../static/ws.html");
//		return page.render(x);
//	});
//
//	app.port(40080)
//		.multithreaded()
//		.run();
//}