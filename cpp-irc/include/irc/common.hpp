#pragma once

#include <regex>
#include <string>
#include <unordered_map>

namespace irc {
class parsing_error : public std::exception {
public:
  std::string raw;

  parsing_error() {
  }

  parsing_error(std::string& r) : raw(std::move(r)) {
  }
};

std::tuple<std::string_view, std::unordered_map<std::string_view, std::string_view>> consumeTags(std::string_view raw) {
  std::unordered_map<std::string_view, std::string_view> tags;

  std::string_view key;
  std::string_view value;

  if (raw[0] != '@') {
    return {raw, std::move(tags)};
  }

  size_t offset = 1;
  size_t length = raw.length();
  for (size_t i = offset; i < length; i++) {
    switch (raw[i]) {
    case '=':
      key = raw.substr(offset, i - offset);
      offset = i + 1;
      break;

    case ';':
    case ' ':
      value = raw.substr(offset, i - offset);
      offset = i + 1;
      tags[key] = value;

      if (raw[i] == ' ') {
        goto end;
      }
      break;
    }
  }

end:
  return {raw.substr(offset), std::move(tags)};
}

namespace regex {
std::regex privmsg{"^:(\\w+)!\\w+@\\S+ PRIVMSG #(\\w+) :"};
std::regex ping{"^PING :(.*)"};
std::regex clearchat{"^:\\S+ CLEARCHAT #(\\w+) :(\\w+)"};
std::regex usernotice{"^:\\S+ USERNOTICE #(\\w+) :"};
std::regex userstate{"^:\\S+ USERSTATE #(\\w+)"};
std::regex roomstate{"^:\\S+ ROOMSTATE #(\\w+)"};
std::regex reconnect{"RECONNECT"};
} // namespace regex

namespace twitch {
namespace tags {
// PRIVMSG
constexpr const char* BADGE_INFO = "badge-info";
constexpr const char* BADGES = "badges";
constexpr const char* BITS = "bits";
constexpr const char* COLOR = "color";
constexpr const char* DISPLAY_NAME = "display-name";
constexpr const char* EMOTES = "emotes";
constexpr const char* ID = "id";
constexpr const char* MESSAGE = "message";
constexpr const char* MOD = "mod";
constexpr const char* ROOM_ID = "room-id";
constexpr const char* SUBSCRIBER = "subscriber";
constexpr const char* TMI_SENT_TS = "tmi-sent-ts";
constexpr const char* TURBO = "turbo";
constexpr const char* USER_ID = "user-id";
constexpr const char* USER_TYPE = "user-type";

// CLEARCHAT
constexpr const char* BAN_DURATION = "ban-duration";
// constexpr const char* ROOM_ID = "room-id";
constexpr const char* TARGET_USER_ID = "target-user-id";
// constexpr const char* TMI_SENT_TS = "tmi-sent-ts";

// USERNOTICE
// constexpr const char* BADGE_INFO = "badge-info";
// constexpr const char* BADGES = "badges";
// constexpr const char* COLOR = "color";
// constexpr const char* DISPLAY_NAME = "display-name";
// constexpr const char* EMOTES = "emotes";
constexpr const char* FLAGS = "flags";
// constexpr const char* ID = "id";
constexpr const char* LOGIN = "login";
// constexpr const char* MOD = "mod";
constexpr const char* MSG_ID = "msg-id";
constexpr const char* MSG_PARAM_CUMULATIVE_MONTHS = "msg-param-cumulative-months";
constexpr const char* MSG_PARAM_MONTHS = "msg-param-months";
constexpr const char* MSG_PARAM_MULTIMONTH_DURATION = "msg-param-multimonth-duration";
constexpr const char* MSG_PARAM_MULTIMONTH_TENURE = "msg-param-multimonth-tenure";
constexpr const char* MSG_PARAM_SHOULD_SHARE_STREAK = "msg-param-should-share-streak";
constexpr const char* MSG_PARAM_SUB_PLAN_NAME = "msg-param-sub-plan-name";
constexpr const char* MSG_PARAM_SUB_PLAN = "msg-param-sub-plan";
constexpr const char* MSG_PARAM_WAS_GIFTED = "msg-param-was-gifted";
// constexpr const char* ROOM_ID = "room-id";
// constexpr const char* SUBSCRIBER = "subscriber";
constexpr const char* SYSTEM_MSG = "system-msg";
// constexpr const char* TMI_SENT_TS = "tmi-sent-ts";
// constexpr const char* USER_ID = "user-id";
// constexpr const char* USER_TYPE = "user-type";

// ROOMSTATE
constexpr const char* EMOTE_ONLY = "emote-only";
constexpr const char* FOLLOWERS_ONLY = "followers-only";
constexpr const char* R9K = "r9k";
constexpr const char* RITUALS = "rituals";
// constexpr const char* ROOM_ID = "room-id";
constexpr const char* SLOW = "slow";
constexpr const char* SUBS_ONLY = "subs-only";

// NOTICE
// constexpr const char* MSG_ID = "msg-id";
} // namespace tags
} // namespace twitch
} // namespace irc
