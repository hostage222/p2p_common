#ifndef P2P_COMMON_H
#define P2P_COMMON_H

#include <cstdint>
#include <string>
#include <array>
#include <utility>
#include <boost/asio.hpp>

namespace p2p
{

struct version_type { int major; int minor; int patch; };

/**
 * @brief Глобально уникальный идентификатор пользователя (контакта);
 * формат строго не определён
 */
using friend_id_type = std::string;

std::string to_string(const boost::asio::ip::tcp::endpoint &endpoint);
std::string to_string(version_type version);

/*
 * Формат сообщения: "команда [данные]\n" (без кавычек);
 * данные могут содержать любое количество параметров, разделённых пробелом;
 * если внутри параметра находятся символы пробела или конца
 * строки, этот параметр выделяется кавычками, кавычки внутри
 * параметра представляются как \", обратная черта как \\
 *
 */
constexpr size_t MAX_BUFFER_SIZE = 10000;
using buffer_type = std::array<uint8_t, MAX_BUFFER_SIZE>;
using buf_iterator = buffer_type::iterator;
using buf_sequence = std::pair<buf_iterator, buf_iterator>;

size_t read_complete(const buffer_type &buf, size_t bytes);
bool is_valid_message(const buffer_type &buf, size_t bytes);

buf_sequence get_buf_sequence(buffer_type &buf, size_t bytes);
bool is_empty(buf_sequence buf);

struct invalid_token_exception {};
buf_sequence read_token(buf_sequence &buf);
std::string token_to_string(buf_sequence buf);
std::string read_string(buf_sequence &buf);

bool write_command(buffer_type &buf, size_t &bytes, const std::string &cmd);
bool append_param(buffer_type &buf, size_t &bytes, const std::string &param);

const std::string GET_VERSION = "GET_VERSION";

const std::string INVALID_FORMAT = "INVALID_FORMAT";
const std::string INVALID_COMMAND = "INVALID_COMMAND";
const std::string INVALID_DATA = "INVALID_DATA";

}

#endif // P2P_COMMON_H
