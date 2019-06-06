	#include "mongo_service.h"
#include "constants.h"
#include "utils.h"

#include <string>
#include <vector>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

MongoService::MongoService(std::shared_ptr<mongocxx::instance> instance,
	std::shared_ptr<mongocxx::pool> pool)
	: _instance(instance)
	, _pool(pool) {
}

std::vector<std::string>
MongoService::FindUsers(const std::string& query) const {
	auto client = _pool->acquire();
	auto usersCollection = (*client)[CHAT_DB][USERS_COLLECTION];

	std::vector<std::string> result;
	auto searchPattern = "^" + query;
	auto findResults = usersCollection.find(make_document(kvp("username", bsoncxx::types::b_regex{ searchPattern })));
	for (auto& item : findResults) {
		auto element = item["username"];
		if (element) {
			auto username = std::string(element.get_utf8().value.data());
			result.push_back(username);
		}
	}

	return result;
}

std::string
MongoService::GetUserId(const std::string& username) const {
	auto client = _pool->acquire();
	auto usersCollection = (*client)[CHAT_DB][USERS_COLLECTION];
	auto findResult = usersCollection.find_one(document{} << "username" << username << finalize);
	if (findResult) {
		auto userId = findResult.value().view()["_id"].get_oid().value.to_string();
		return userId;
	}

	return "-1";
}

std::string
MongoService::GetUsername(const bsoncxx::oid& userId) const {
	auto client = _pool->acquire();
	auto usersCollection = (*client)[CHAT_DB][USERS_COLLECTION];
	auto userDocument = usersCollection.find_one(document{} << "_id" << userId << finalize);
	return PullUsername(userDocument.value());
}

void MongoService::InsertMessage(const bsoncxx::oid & senderId,
	const bsoncxx::oid & recieverId,
	const std::string & message,
	const bsoncxx::types::b_date & timeStamp) const {
	auto client = _pool->acquire();
	auto conversationsCollection = (*client)[CHAT_DB][CONVERSATIONS_COLLECTION];

	std::pair<bsoncxx::oid, bsoncxx::oid> users = std::make_pair(senderId, recieverId);
	if (senderId > recieverId) {
		std::swap(users.first, users.second);
	}

	document data_builder{};
	data_builder << "users" << open_array << users.first << users.second
		<< close_array << "sender" << senderId << "timestamp"
		<< timeStamp << "message" << message;

	conversationsCollection.insert_one(data_builder.view());
}

bool MongoService::IsUserAdded(const bsoncxx::oid & userId) const {
	auto client = _pool->acquire();
	auto usersCollection = (*client)[CHAT_DB][USERS_COLLECTION];
	return IsUserAdded(userId, &usersCollection);
}

std::vector<std::string>
MongoService::LoadFriends(const bsoncxx::oid & userId) const {
	auto client = _pool->acquire();
	auto usersCollection = (*client)[CHAT_DB][USERS_COLLECTION];
	auto friendsCollection = (*client)[CHAT_DB][FRIENDS_COLLECTION];

	std::vector<std::string> result;
	auto userDocument = friendsCollection.find_one(document{} << "_id" << userId << finalize);
	if (userDocument) {
		auto element = userDocument.value().view()["friends"];
		if (element) {
			auto userFriends = element.get_array().value;
			for (auto& item : userFriends) {
				auto userFriendId = item.get_oid().value;
				auto userFriendDocument = usersCollection.find_one(document{} << "_id" << userFriendId << finalize);
				if (userFriendDocument) {
					auto userFriendName = PullUsername(userFriendDocument.value());
					result.push_back(userFriendName);
				}
			}
		}
	}

	return result;
}

std::vector<Message>
MongoService::LoadMessages(const bsoncxx::oid & firstUserId,
	const bsoncxx::oid & secondUserId,
	const bsoncxx::types::b_date & timeStamp,
	int messagesNumber) const {
	auto client = _pool->acquire();
	auto conversationsCollection = (*client)[CHAT_DB][CONVERSATIONS_COLLECTION];

	auto order = document{} << "timestamp" << -1 << finalize;
	auto opts = mongocxx::options::find{};
	opts.sort(order.view());
	opts.limit(messagesNumber);

	std::pair<bsoncxx::oid, bsoncxx::oid> users = std::make_pair(firstUserId, secondUserId);
	if (firstUserId > secondUserId) {
		std::swap(users.first, users.second);
	}

	auto x = GetUsername(firstUserId);
	auto messages = conversationsCollection.find(
		document{} << "users" << open_array << users.first << users.second
		<< close_array << "timestamp" << open_document << "$gt"
		<< timeStamp << close_document << finalize,
		opts);

	std::vector<Message> result;
	for (auto& item : messages) {
		auto senderId = item["sender"].get_oid().value;
		auto senderName = GetUsername(senderId);

		Message message(
			senderName,
			std::string(item["message"].get_utf8().value.data()),
			item["timestamp"].get_date().to_int64()
		);
		result.push_back(message);
	}

	return result;
}

void MongoService::MakeFriends(const bsoncxx::oid & firstUserId, const bsoncxx::oid & secondUserId) {
	auto client = _pool->acquire();
	auto friendsCollection = (*client)[CHAT_DB][FRIENDS_COLLECTION];

	_addFriendMutex.lock();
	friendsCollection.update_one(
		document{} << "_id" << firstUserId << finalize,
		document{} << "$addToSet" << open_document
		<< "friends" << secondUserId
		<< close_document << finalize
	);
	friendsCollection.update_one(
		document{} << "_id" << secondUserId << finalize,
		document{} << "$addToSet" << open_document
		<< "friends" << firstUserId
		<< close_document << finalize
	);
	_addFriendMutex.unlock();
}

void MongoService::TryAddUser(const std::string & username) const {
	auto client = _pool->acquire();
	auto usersCollection = (*client)[CHAT_DB][USERS_COLLECTION];
	auto friendsCollection = (*client)[CHAT_DB][FRIENDS_COLLECTION];
	if (!IsUserAdded(username, &usersCollection)) {
		auto userDocument = CreateUserDocument(username);
		auto insertResult = usersCollection.insert_one(userDocument.view());
		auto insertedId = insertResult.value().inserted_id().get_oid().value;
		auto friendsDocument = CreateFriendsDocument(insertedId);
		friendsCollection.insert_one(friendsDocument.view());
	}
}

void MongoService::TryAddUsers() const {
	auto client = _pool->acquire();
	auto usersCollection = (*client)[CHAT_DB][USERS_COLLECTION];
	auto friendsCollection = (*client)[CHAT_DB][FRIENDS_COLLECTION];
	auto conversationsCollection = (*client)[CHAT_DB][CONVERSATIONS_COLLECTION];

	if (usersCollection.count_documents({}) != USERS_NUMBER || friendsCollection.count_documents({}) != USERS_NUMBER) {
		usersCollection.delete_many({});
		friendsCollection.delete_many({});
		AddUsers(&usersCollection);
		auto users = usersCollection.find({});
		for (auto& item : users) {
			auto userId = item["_id"].get_oid().value;

			auto usersInternal = usersCollection.find({});
			auto friendsDocument = CreateFriendsDocument(userId, usersInternal);
			friendsCollection.insert_one(friendsDocument.view());
		}
	}
}

void MongoService::AddUsers(mongocxx::v_noabi::collection * usersCollection) const {
	for (int i = 0; i < USERS_NUMBER; ++i) {
		auto username = "User" + std::to_string(i + 1);

		document document{};
		document << "username" << username;
		usersCollection->insert_one(document.view());
	}
}

const bsoncxx::v_noabi::document::value
MongoService::CreateFriendsDocument(const bsoncxx::oid & userId) const {
	document dataBuilder{};
	dataBuilder << "_id" << userId;
	auto arrayBuilder = dataBuilder << "friends" << open_array << close_array;
	auto document = dataBuilder << finalize;

	return document;
}

const bsoncxx::v_noabi::document::value
MongoService::CreateFriendsDocument(const bsoncxx::oid & userId,
	mongocxx::cursor & users) const {
	document dataBuilder{};
	dataBuilder << "_id" << userId;
	auto arrayBuilder = dataBuilder << "friends" << open_array;
	for (auto& item : users) {
		if (item["_id"].get_oid().value != userId) {
			arrayBuilder << item["_id"].get_oid().value;
		}
	}
	arrayBuilder << close_array;
	auto document = dataBuilder << finalize;

	return document;
}

const bsoncxx::v_noabi::document::value
MongoService::CreateUserDocument(const std::string & username) const {
	document dataBuilder{};
	dataBuilder << "username" << username;
	auto res = dataBuilder << finalize;

	return res;
}

std::string
MongoService::PullUsername(
	const bsoncxx::v_noabi::document::value & userDocument) const {
	return std::string(userDocument.view()["username"].get_utf8().value.data());
}

bool MongoService::IsUserAdded(const std::string & username,
	mongocxx::v_noabi::collection * usersCollection) const {
	return usersCollection->find_one(document{}
		<< "username" << username
		<< finalize)
		!= bsoncxx::stdx::nullopt;
}

bool MongoService::IsUserAdded(const bsoncxx::oid & userId,
	mongocxx::v_noabi::collection * usersCollection) const {
	return usersCollection->find_one(document{} << "_id" << userId << finalize) != bsoncxx::stdx::nullopt;
}
