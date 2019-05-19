#pragma once

#include "utils.h"

#include <iostream>
#include <mutex>
#include <string>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

using bsoncxx::builder::stream::document;

class MongoService {
private:
	std::shared_ptr<mongocxx::instance> _instance;
	std::shared_ptr<mongocxx::pool> _pool;

	std::mutex _addFriendMutex;

public:
	MongoService(std::shared_ptr<mongocxx::instance>,
		std::shared_ptr<mongocxx::pool>);

	std::vector<std::string> FindUsers(const std::string&) const;
	std::string GetUserId(const std::string&) const;
	std::string GetUsername(const bsoncxx::oid&) const;
	void InsertMessage(const bsoncxx::oid&,
		const bsoncxx::oid&,
		const std::string&,
		const bsoncxx::types::b_date&) const;
	bool IsUserAdded(const bsoncxx::oid&) const;
	std::vector<std::string> LoadFriends(const bsoncxx::oid&) const;
	std::vector<Message> LoadMessages(const bsoncxx::oid&,
		const bsoncxx::oid&,
		const bsoncxx::types::b_date&,
		int) const;
	void MakeFriends(const bsoncxx::oid&, const bsoncxx::oid&);
	void TryAddUser(const std::string&) const;
	void TryAddUsers() const;

private:
	void AddUsers(mongocxx::v_noabi::collection*) const;
	const bsoncxx::v_noabi::document::value CreateFriendsDocument(
		const bsoncxx::oid&) const;
	const bsoncxx::v_noabi::document::value CreateFriendsDocument(
		const bsoncxx::oid&,
		mongocxx::cursor&) const;
	const bsoncxx::v_noabi::document::value CreateUserDocument(
		const std::string&) const;
	std::string PullUsername(const bsoncxx::v_noabi::document::value&) const;
	bool IsUserAdded(const std::string&, mongocxx::v_noabi::collection*) const;
	bool IsUserAdded(const bsoncxx::oid&, mongocxx::v_noabi::collection*) const;
};
