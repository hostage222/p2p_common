#include "p2p_common.h"

#include <cstdint>
#include <string>
#include <array>
#include <utility>
#include <boost/asio.hpp>

#include <cstring>
#include <limits>

using namespace std;

namespace p2p
{

size_t analyze_buf(const buffer_type &buf, size_t bytes, bool *valid = 0);
bool has_special_symbols(const string &param);
bool append_symbol(buffer_type &buf, size_t &bytes, char symbol);
int read_version_num(const string &s, size_t &start);

string to_string(const boost::asio::ip::tcp::endpoint &endpoint)
{
    return endpoint.address().to_string() + ":" +
            std::to_string(endpoint.port());
}

string to_string(version_type version)
{
    return std::to_string(version.major) + "." +
            std::to_string(version.minor) + "." +
            std::to_string(version.patch);
}

int string_to_int(const string &s, bool &ok)
{
    if (s.empty())
    {
        ok = false;
        return 0;
    }

    const char *c = s.c_str();
    char *end;

    long int r = strtol(c, &end, 10);

    if (end != c + s.size())
    {
        ok = false;
        return 0;
    }

    if (r < numeric_limits<int>::min() ||
        r > numeric_limits<int>::max())
    {
        ok = false;
        return 0;
    }

    ok = true;
    return r;
}

version_type string_to_version(const string &s)
{
    size_t start = 0;
    version_type res;
    res.major = read_version_num(s, start);
    res.minor = read_version_num(s, start);
    res.patch = read_version_num(s, start);
    if (start < s.size())
    {
        throw invalid_version_format_exception{};
    }
    return res;
}

size_t read_complete(const buffer_type &buf, size_t bytes)
{
    return analyze_buf(buf, bytes);
}

bool is_valid_message(const buffer_type &buf, size_t bytes)
{
    bool res;
    analyze_buf(buf, bytes, &res);
    return res;
}

buf_sequence get_buf_sequence(const buffer_type &buf, size_t bytes)
{
    if (bytes == 0)
    {
        return buf_sequence{buf.cbegin(), buf.cbegin()};
    }

    if (buf[bytes - 1] == '\n')
    {
        return buf_sequence{buf.cbegin(), buf.cbegin() + bytes - 1};
    }
    else
    {
        return buf_sequence{buf.cbegin(), buf.cbegin() + bytes};
    }
}

bool is_empty(buf_sequence buf)
{
    return buf.first == buf.second;
}

buf_sequence read_token(buf_sequence &buf)
{
    if (buf.first == buf.second)
    {
        throw invalid_token_exception{};
    }

    buf_sequence res;
    if (*buf.first == '"')
    {
        ++buf.first;
        res.first = buf.first;

        bool spec_symbol = false;
        while (buf.first != buf.second)
        {
            char c = *buf.first;
            if (spec_symbol)
            {
                if (c == '\\' || c == '*')
                {
                    spec_symbol = false;
                }
                else
                {
                    throw invalid_token_exception{};
                }
            }
            else
            {
                if (c == '"')
                {
                    break;
                }
                else if (c == '\\')
                {
                    spec_symbol = true;
                }
            }
            ++buf.first;
        }

        if (buf.first != buf.second)
        {
            res.second = buf.first;
            if (res.first == res.second)
            {
                throw invalid_token_exception{};
            }
            ++buf.first;
            if (buf.first != buf.second)
            {
                if (*buf.first == ' ')
                {
                    ++buf.first;
                }
                else
                {
                    throw invalid_token_exception{};
                }
            }
        }
        else
        {
            throw invalid_token_exception{};
        }
    }
    else
    {
        auto it = find(buf.first, buf.second, ' ');
        res.first = buf.first;
        res.second = it;
        if (it != buf.second)
        {
            buf.first = next(it);
        }
        else
        {
            buf.first = it;
        }
    }
    return res;
}

string token_to_string(buf_sequence buf)
{
    if (buf.first == buf.second)
    {
        throw invalid_token_exception{};
    }

    if (*buf.first == '"')
    {
        string res;
        bool spec_symbol = false;
        while (buf.first != buf.second)
        {
            char c = *buf.first;
            if (spec_symbol)
            {
                if (c == '\\' || c == '*')
                {
                    res.push_back(c);
                    spec_symbol = false;
                }
                else
                {
                    throw invalid_token_exception{};
                }
            }
            else
            {
                if (c == '"')
                {
                    break;
                }
                else if (c == '\\')
                {
                    spec_symbol = true;
                }
                else
                {
                    res.push_back(c);
                }
            }
            ++buf.first;
        }

        if (buf.first != buf.second && next(buf.first) == buf.second)
        {
            return res;
        }
        else
        {
            throw invalid_token_exception{};
        }
    }
    else
    {
        auto it = find(buf.first, buf.second, ' ');
        if (it == buf.second)
        {
            return string{buf.first, buf.second};
        }
        else
        {
            throw invalid_token_exception{};
        }
    }
}

string read_string(buf_sequence &buf)
{
    return token_to_string(read_token(buf));
}

bool write_command(buffer_type &buf, size_t &bytes, const string &cmd)
{
    if (cmd.size() > buf.size())
    {
        return false;
    }

    bytes = cmd.size();
    copy(cmd.begin(), cmd.end(), buf.begin());
    return true;
}

bool append_param(buffer_type &buf, size_t &bytes, const string &param)
{
    size_t new_size = bytes;
    if (!append_symbol(buf, new_size, ' '))
    {
        return false;
    }

    if (has_special_symbols(param))
    {
        if (!append_symbol(buf, new_size, '"'))
        {
            return false;
        }

        for (char c : param)
        {
            if (c == '\\')
            {
                if (!append_symbol(buf, new_size, '\\') ||
                    !append_symbol(buf, new_size, '\\'))
                {
                    return false;
                }
            }
            else if (c == '*')
            {
                if (!append_symbol(buf, new_size, '*') ||
                    !append_symbol(buf, new_size, '*'))
                {
                    return false;
                }
            }
            else
            {
                if (!append_symbol(buf, new_size, c))
                {
                    return false;
                }
            }
        }

        if (!append_symbol(buf, new_size, '"'))
        {
            return false;
        }
    }
    else
    {
        if (new_size + param.size() > buf.size())
        {
            return false;
        }

        copy(param.begin(), param.end(), buf.begin() + new_size);
        new_size += param.size();
    }

    bytes = new_size;
    return true;
}

bool finalize(buffer_type &buf, size_t &bytes)
{
    return append_symbol(buf, bytes, '\n');
}

size_t analyze_buf(const buffer_type &buf, size_t bytes, bool *valid)
{
    bool spec_symbol = false;
    bool raw_input = false;
    bool v = true;
    size_t res = 1;

    if (bytes == 0)
    {
        return 1;
    }

    for (size_t i = 0; i < bytes; ++i)
    {
        uint8_t d = buf[i];
        if (spec_symbol)
        {
            if (d != '\\' && d != '*')
            {
                v = false;
                res = 0;
                break;
            }
            spec_symbol = false;
        }
        else if (raw_input)
        {
            if (d == '\\')
            {
                spec_symbol = true;
            }
            else if (d == '*')
            {
                raw_input = false;
            }
        }
        else
        {
            if (d == '*')
            {
                raw_input = true;
            }
        }
    }

    if (buf[bytes - 1] == '\n')
    {
        res = 0;
    }
    else if (bytes >= MAX_BUFFER_SIZE)
    {
        v = false;
        res = 0;
    }

    if (valid)
    {
        *valid = v;
    }
    return res;
}

bool has_special_symbols(const string &param)
{
    return param.find_first_of(" \n*") != string::npos;
}

bool append_symbol(buffer_type &buf, size_t &bytes, char symbol)
{
    if (bytes >= buf.size())
    {
        return false;
    }

    buf[bytes++] = symbol;
    return true;
}

int read_version_num(const string &s, size_t &start)
{
    size_t p = s.find('.', start);
    if (p == string::npos)
    {
        p = s.size();
    }

    bool ok;
    int res = string_to_int(s.substr(start, p - start), ok);
    if (ok)
    {
        start = p + 1;
        return res;
    }
    else
    {
        throw invalid_version_format_exception{};
    }
}

}//p2p
