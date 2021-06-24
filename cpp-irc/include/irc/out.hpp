#pragma once

#include <string>

namespace irc::out {
using namespace std::string_literals;

std::string pass(std::string_view pass) {
  return "PASS "s + (std::string)pass + "\r\n";
}

std::string nick(std::string_view nick) {
  return "NICK "s + (std::string)nick + "\r\n";
}

std::string pong(std::string_view src) {
  return "PONG :"s + (std::string)src + "\r\n";
}

std::string privmsg(std::string_view chn, std::string_view msg) {
  return "PRIVMSG #"s + (std::string)chn + " :"s + (std::string)msg + "\r\n";
}

std::string join(std::string_view chn) {
  return "JOIN #"s + (std::string)chn + "\r\n";
}

std::string part(std::string_view chn) {
  return "PART #"s + (std::string)chn + "\r\n";
}

std::string capreq(std::string_view cap) {
  return "CAP REQ :"s + (std::string)cap + "\r\n";
}

namespace twitch {
std::string nickAnon() {
  return nick("justinfan93434586");
}

std::string whisper(std::string_view chn, std::string_view usr, std::string_view msg) {
  return privmsg(chn, "/w "s + (std::string)usr + " "s + (std::string)msg);
}

std::string capreqMembership() {
  return capreq("twitch.tv/membership");
}

std::string capreqTags() {
  return capreq("twitch.tv/tags");
}

std::string capreqCommands() {
  return capreq("twitch.tv/commands");
}
} // namespace twitch
} // namespace irc::out
