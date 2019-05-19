#pragma once

#include <string>

const bool IS_TESTING = true;

const int CROW_PORT = 18080;

const std::string HTML_PAGES_PATH = "../../static";
const std::string SIGN_IN_PAGE = "/sign_in_page.html";
const std::string CHAT_PAGE = "/chat.html";

const int USERS_NUMBER = 10;

const std::string CHAT_DB_URI = "mongodb://localhost:27017";

const std::string CHAT_DB = "chat";
const std::string USERS_COLLECTION = "users";
const std::string FRIENDS_COLLECTION = "friends";
const std::string CONVERSATIONS_COLLECTION = "conversations";
