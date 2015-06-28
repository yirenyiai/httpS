//
// Copyright (C) 2013 Jack.
//
// Author: jack
// Email:  jack.wgm@gmail.com
//

#pragma once

#include <boost/regex.hpp>

#include "internal.hpp"
#include "escape_string.hpp"
#include <boost/algorithm/string.hpp>

#ifndef atoi64
# ifdef _MSC_VER
#  define atoi64 _atoi64
#  define strncasecmp _strnicmp
# else
#  define atoi64(x) strtoll(x, (char**)NULL, 10)
# endif
#endif // atoi64

namespace http
{
	template <typename Iterator>
	bool parse_http_status_line(Iterator begin, Iterator end,
		int& version_major, int& version_minor, int& status)
	{
		using namespace detail;
		enum
		{
			http_version_h,
			http_version_t_1,
			http_version_t_2,
			http_version_p,
			http_version_slash,
			http_version_major_start,
			http_version_major,
			http_version_minor_start,
			http_version_minor,
			status_code_start,
			status_code,
			reason_phrase,
			linefeed,
			fail
		} state = http_version_h;

		Iterator iter = begin;
		std::string reason;
		while (iter != end && state != fail)
		{
			char c = *iter++;
			switch (state)
			{
			case http_version_h:
				state = (c == 'H') ? http_version_t_1 : fail;
				break;
			case http_version_t_1:
				state = (c == 'T') ? http_version_t_2 : fail;
				break;
			case http_version_t_2:
				state = (c == 'T') ? http_version_p : fail;
				break;
			case http_version_p:
				state = (c == 'P') ? http_version_slash : fail;
				break;
			case http_version_slash:
				state = (c == '/') ? http_version_major_start : fail;
				break;
			case http_version_major_start:
				if (is_digit(c))
				{
					version_major = version_major * 10 + c - '0';
					state = http_version_major;
				}
				else
					state = fail;
				break;
			case http_version_major:
				if (c == '.')
					state = http_version_minor_start;
				else if (is_digit(c))
					version_major = version_major * 10 + c - '0';
				else
					state = fail;
				break;
			case http_version_minor_start:
				if (is_digit(c))
				{
					version_minor = version_minor * 10 + c - '0';
					state = http_version_minor;
				}
				else
					state = fail;
				break;
			case http_version_minor:
				if (c == ' ')
					state = status_code_start;
				else if (is_digit(c))
					version_minor = version_minor * 10 + c - '0';
				else
					state = fail;
				break;
			case status_code_start:
				if (is_digit(c))
				{
					status = status * 10 + c - '0';
					state = status_code;
				}
				else
					state = fail;
				break;
			case status_code:
				if (c == ' ')
					state = reason_phrase;
				else if (is_digit(c))
					status = status * 10 + c - '0';
				else
					state = fail;
				break;
			case reason_phrase:
				if (c == '\r')
					state = linefeed;
				else if (is_ctl(c))
					state = fail;
				else
					reason.push_back(c);
				break;
			case linefeed:
				return (c == '\n');
			default:
				return false;
			}
		}
		return false;
	}

	inline bool headers_equal(const std::string& a, const std::string& b)
	{
		if (a.length() != b.length())
			return false;
		return std::equal(a.begin(), a.end(), b.begin(), detail::tolower_compare);
	}

	inline void check_header(const std::string& name, const std::string& value,
		std::string& content_type, boost::int64_t& content_length,
		std::string& location)
	{
		if (headers_equal(name, "Content-Type"))
			content_type = value;
		else if (headers_equal(name, "Content-Length"))
			content_length = (std::max)((boost::int64_t)atoi64(value.c_str()), content_length);
		else if (headers_equal(name, "Location"))
			location = value;
		else if (headers_equal(name, "Content-Range"))
		{
			std::string::size_type f = value.find('/');
			if (f != std::string::npos && f++ != std::string::npos)
			{
				std::string tmp = value.substr(f);
				if (!tmp.empty())
				{
					boost::int64_t length = atoi64(tmp.c_str());
					content_length = (std::max)(length, content_length);
				}
			}
		}
	}

	typedef std::vector<std::pair<std::string, std::string> > http_headers;

	template <typename Iterator>
	bool parse_http_headers(Iterator begin, Iterator end,
		std::string& content_type, boost::int64_t& content_length,
		std::string& location, http_headers& headers)
	{
		using namespace detail;
		enum
		{
			first_header_line_start,
			header_line_start,
			header_lws,
			header_name,
			space_before_header_value,
			header_value,
			linefeed,
			final_linefeed,
			fail
		} state = first_header_line_start;

		Iterator iter = begin;
		std::string reason;
		std::string name;
		std::string value;
		while (iter != end && state != fail)
		{
			char c = *iter++;
			switch (state)
			{
			case first_header_line_start:
				if (c == '\r')
					state = final_linefeed;
				else if (!is_char(c) || is_ctl(c) || is_tspecial(c))
					state = fail;
				else
				{
					name.push_back(c);
					state = header_name;
				}
				break;
			case header_line_start:
				if (c == '\r')
				{
					boost::trim(name);
					boost::trim(value);
					check_header(name, value, content_type, content_length, location);
					headers.push_back(std::make_pair(name, value));
					name.clear();
					value.clear();
					state = final_linefeed;
				}
				else if (c == ' ' || c == '\t')
					state = header_lws;
				else if (!is_char(c) || is_ctl(c) || is_tspecial(c))
					state = fail;
				else
				{
					boost::trim(name);
					boost::trim(value);
					check_header(name, value, content_type, content_length, location);
					headers.push_back(std::make_pair(name, value));
					name.clear();
					value.clear();
					name.push_back(c);
					state = header_name;
				}
				break;
			case header_lws:
				if (c == '\r')
					state = linefeed;
				else if (c == ' ' || c == '\t')
					; // Discard character.
				else if (is_ctl(c))
					state = fail;
				else
				{
					state = header_value;
					value.push_back(c);
				}
				break;
			case header_name:
				if (c == ':')
					state = space_before_header_value;
				else if (!is_char(c) || is_ctl(c) || is_tspecial(c))
					state = fail;
				else
					name.push_back(c);
				break;
			case space_before_header_value:
				if (c == ' ')
					state = header_value;
				if (c == '\r')	// 当value没有值的时候, 直接进入读取value完成逻辑, 避免失败.
					state = linefeed;
				else if (is_ctl(c))
					state = fail;
				else
				{
					value.push_back(c);
					state = header_value;
				}
				break;
			case header_value:
				if (c == '\r')
					state = linefeed;
				else if (is_ctl(c))
					state = fail;
				else
					value.push_back(c);
				break;
			case linefeed:
				state = (c == '\n') ? header_line_start : fail;
				break;
			case final_linefeed:
				return (c == '\n');
			default:
				return false;
			}
		}
		return false;
	}

	/// struct header.
	struct header
	{
		std::string name;
		std::string value;
	};


	/// A request received from a client.
	struct request
	{
		std::string method;
		std::string uri;
		std::vector<std::pair<std::string, std::string>> uri_params;		// 不关是get还是post，都有可能在uri中带参数。

		int http_version_major;
		int http_version_minor;
		std::vector<header> headers;

		// 只有在调用了 normalise 后才能访问的成员
		boost::uint64_t content_length;
		bool keep_alive;

		std::string body;

		std::string operator[](const std::string& name) const
		{
			for (const header& hdr : headers)
			{
				if (strncasecmp(name.c_str(), hdr.name.c_str(), name.length()) == 0)
				{
					return hdr.value;
				}
			}
			return "";
		}

		// 将一些标准头部从 headers 提取出来
		// 必要的小写化
		void normalise()
		{
			boost::to_lower(method);
			boost::to_lower(uri);
			for (header& hdr : headers) boost::to_lower(hdr.name);
			auto contentlength = (*this)["content-length"];
			if (!contentlength.empty())
			{
				std::istringstream contentlength_string;
				contentlength_string.str(contentlength);
				contentlength_string.imbue(std::locale("C"));
				contentlength_string >> content_length;
			}

			keep_alive = boost::to_lower_copy((*this)["connection"]) == "keep-alive";
		}
	};

	// HTTP 表单
	// application/x-www-form-urlencoded
	// multipart/form-data; boundary=xxxx
	struct http_form
	{
		http_form(const std::string& formdata, const std::string& content_type)
		{
			boost::smatch w;
			if(boost::regex_search(content_type, w, boost::regex("boundary=([^ ]+)")))
			{
				std::string boundary = w[1];
				boundary.insert(0, "--");
				parse_multipart(formdata, boundary);
			}
			else
			{
				parse_form_string(formdata);
			}
		}

		std::string operator[](const std::string& key) const
		{
			for (const auto& hdr : headers)
			{
				if (hdr.first == key)
				{
					return hdr.second;
				}
			}
			return "";
		}
	private:
		void parse_multipart(const std::string& formdata, const std::string& boundary)
		{
			std::vector<std::string> parts;

			std::size_t boundary_start_post, search_pos = boundary.length() + 2;

			while(std::string::npos != (boundary_start_post = formdata.find(boundary, search_pos)))
			{
				parts.push_back(formdata.substr(search_pos, boundary_start_post - search_pos - 2));
				search_pos = boundary_start_post + boundary.length() + 2;
			}

			// 从parts里提取
			for (auto p : parts)
			{
				auto pos = p.find("\r\n\r\n");
				auto h = p.substr(0, pos);
				auto v = p.substr(pos + 4);

				boost::smatch w;
				if(boost::regex_search(h, w, boost::regex("Content-Disposition: form-data; name=\"([^\"]+)\"")))
				{
					std::string key = w[1];
					detail::unescape_path(key, h);
					headers.push_back(std::make_pair(h,v));
				}
			}
		}

		void parse_form_string(const std::string& formdata)
		{
			std::string key, value;
			bool parse_key = true;
			for ( auto C : formdata)
			{
				if (parse_key)
				{
					if (C != '=')
					{
						key+=C;
					}
					else
					{
						parse_key = false;
					}
				}
				else
				{
					if (C != '&')
					{
						value+=C;
					}
					else
					{
						parse_key = true;

						std::string k,v;

						detail::unescape_path(key, k);
						detail::unescape_path(value, v);

						headers.push_back(std::make_pair(k,v));
						key = value = "";
					}
				}
			}
			if (!parse_key)
			{
				std::string k,v;

				detail::unescape_path(key, k);
				detail::unescape_path(value, v);

				headers.push_back(std::make_pair(k,v));
			}
		}
		std::vector<std::pair<std::string, std::string>> headers;
	};

	/// Parser for incoming requests.
	class request_parser
	{
	public:
		/// Construct ready to parse the request method.
		request_parser()
			: state_(method_start)
		{}

		/// Reset to initial parser state.
		void reset()
		{
			state_ = method_start;
		}

		/// Parse some data. The tribool return value is true when a complete request
		/// has been parsed, false if the data is invalid, indeterminate when more
		/// data is required. The InputIterator return value indicates how much of the
		/// input has been consumed.
		template <typename InputIterator>
		boost::tuple<boost::tribool, InputIterator> parse(request& req,
			InputIterator begin, InputIterator end)
		{
			while (begin != end)
			{
				boost::tribool result = consume(req, *begin++);
				if (result || !result)
					return boost::make_tuple(result, begin);
			}
			boost::tribool result = boost::indeterminate;
			return boost::make_tuple(result, begin);
		}

	private:
		/// Handle the next character of input.
		boost::tribool consume(request& req, char input)
		{
			switch (state_)
			{
			case method_start:
				if (!is_char(input) || is_ctl(input) || is_tspecial(input))
				{
					return false;
				}
				else
				{
					state_ = method;
					req.method.push_back(input);
					return boost::indeterminate;
				}
			case method:
				if (input == ' ')
				{
					state_ = uri;
					return boost::indeterminate;
				}
				else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
				{
					return false;
				}
				else
				{
					req.method.push_back(input);
					return boost::indeterminate;
				}
			case uri_start:
				if (is_ctl(input))
				{
					return false;
				}
				else
				{
					state_ = uri;
					req.uri.push_back(input);
					return boost::indeterminate;
				}
			case uri:
				if (input == ' ')
				{
					state_ = http_version_h;
					return boost::indeterminate;
				}
				else if (is_ctl(input))
				{
					return false;
				}
				else if (input == '?')
				{
					state_ = uri_params_key;
					req.uri_params.push_back(std::pair<std::string, std::string>());
					return boost::indeterminate;
				}
				else
				{
					req.uri.push_back(input);
					return boost::indeterminate;
				}
			case uri_params_key:
				{
					if (input == ' ')
					{
						state_ = http_version_h;
						return boost::indeterminate;
					}
					else if (is_ctl(input))
					{
						return false;
					}
					else if (input == '=')
					{
						// 标准的HTTP协议中，如果参数名字存在等号。应该转成URLencode
						state_ = uri_params_value;
						return boost::indeterminate;
					}
					else
					{
						req.uri_params.rbegin()->first.push_back(input);
						return boost::indeterminate;
					}
				}
			case uri_params_value:
				{
					if (input == ' ')
					{
						state_ = http_version_h;
						return boost::indeterminate;
					}
					else if (is_ctl(input))
					{
						return false;
					}
					else if (input == '&')
					{
						// 标准的HTTP协议中，如果参数名字存在 & 。应该转成URLencode
						state_ = uri_params_key;
						req.uri_params.push_back(std::pair<std::string, std::string>());
						return boost::indeterminate;
					}
					else
					{
						req.uri_params.rbegin()->second.push_back(input);
						return boost::indeterminate;
					}
				}
			case http_version_h:
				if (input == 'H')
				{
					state_ = http_version_t_1;
					return boost::indeterminate;
				}
				else
				{
					return false;
				}
			case http_version_t_1:
				if (input == 'T')
				{
					state_ = http_version_t_2;
					return boost::indeterminate;
				}
				else
				{
					return false;
				}
			case http_version_t_2:
				if (input == 'T')
				{
					state_ = http_version_p;
					return boost::indeterminate;
				}
				else
				{
					return false;
				}
			case http_version_p:
				if (input == 'P')
				{
					state_ = http_version_slash;
					return boost::indeterminate;
				}
				else
				{
					return false;
				}
			case http_version_slash:
				if (input == '/')
				{
					req.http_version_major = 0;
					req.http_version_minor = 0;
					state_ = http_version_major_start;
					return boost::indeterminate;
				}
				else
				{
					return false;
				}
			case http_version_major_start:
				if (is_digit(input))
				{
					req.http_version_major = req.http_version_major * 10 + input - '0';
					state_ = http_version_major;
					return boost::indeterminate;
				}
				else
				{
					return false;
				}
			case http_version_major:
				if (input == '.')
				{
					state_ = http_version_minor_start;
					return boost::indeterminate;
				}
				else if (is_digit(input))
				{
					req.http_version_major = req.http_version_major * 10 + input - '0';
					return boost::indeterminate;
				}
				else
				{
					return false;
				}
			case http_version_minor_start:
				if (is_digit(input))
				{
					req.http_version_minor = req.http_version_minor * 10 + input - '0';
					state_ = http_version_minor;
					return boost::indeterminate;
				}
				else
				{
					return false;
				}
			case http_version_minor:
				if (input == '\r')
				{
					state_ = expecting_newline_1;
					return boost::indeterminate;
				}
				else if (is_digit(input))
				{
					req.http_version_minor = req.http_version_minor * 10 + input - '0';
					return boost::indeterminate;
				}
				else
				{
					return false;
				}
			case expecting_newline_1:
				if (input == '\n')
				{
					state_ = header_line_start;
					return boost::indeterminate;
				}
				else
				{
					return false;
				}
			case header_line_start:
				if (input == '\r')
				{
					state_ = expecting_newline_3;
					return boost::indeterminate;
				}
				else if (!req.headers.empty() && (input == ' ' || input == '\t'))
				{
					state_ = header_lws;
					return boost::indeterminate;
				}
				else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
				{
					return false;
				}
				else
				{
					req.headers.push_back(header());
					req.headers.back().name.push_back(input);
					state_ = header_name;
					return boost::indeterminate;
				}
			case header_lws:
				if (input == '\r')
				{
					state_ = expecting_newline_2;
					return boost::indeterminate;
				}
				else if (input == ' ' || input == '\t')
				{
					return boost::indeterminate;
				}
				else if (is_ctl(input))
				{
					return false;
				}
				else
				{
					state_ = header_value;
					req.headers.back().value.push_back(input);
					return boost::indeterminate;
				}
			case header_name:
				if (input == ':')
				{
					state_ = space_before_header_value;
					return boost::indeterminate;
				}
				else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
				{
					return false;
				}
				else
				{
					req.headers.back().name.push_back(input);
					return boost::indeterminate;
				}
			case space_before_header_value:
				if (input == ' ')
				{
					state_ = header_value;
					return boost::indeterminate;
				}
				else
				{
					return false;
				}
			case header_value:
				if (input == '\r')
				{
					state_ = expecting_newline_2;
					return boost::indeterminate;
				}
				else if (is_ctl(input))
				{
					return false;
				}
				else
				{
					req.headers.back().value.push_back(input);
					return boost::indeterminate;
				}
			case expecting_newline_2:
				if (input == '\n')
				{
					state_ = header_line_start;
					return boost::indeterminate;
				}
				else
				{
					return false;
				}
			case expecting_newline_3:
				return (input == '\n');
			default:
				return false;
			}
		}

		/// Check if a byte is an HTTP character.
		static bool is_char(int c)
		{
			return c >= 0 && c <= 127;
		}

		/// Check if a byte is an HTTP control character.
		static bool is_ctl(int c)
		{
			return (c >= 0 && c <= 31) || (c == 127);
		}

		/// Check if a byte is defined as an HTTP tspecial character.
		static bool is_tspecial(int c)
		{
			switch (c)
			{
			case '(': case ')': case '<': case '>': case '@':
			case ',': case ';': case ':': case '\\': case '"':
			case '/': case '[': case ']': case '?': case '=':
			case '{': case '}': case ' ': case '\t':
				return true;
			default:
				return false;
			}
		}

		/// Check if a byte is a digit.
		static bool is_digit(int c)
		{
			return c >= '0' && c <= '9';
		}

		/// The current state of the parser.
		enum state
		{
			method_start,
			method,
			uri_start,
			uri,
			uri_params_key,
			uri_params_value,
			http_version_h,
			http_version_t_1,
			http_version_t_2,
			http_version_p,
			http_version_slash,
			http_version_major_start,
			http_version_major,
			http_version_minor_start,
			http_version_minor,
			expecting_newline_1,
			header_line_start,
			header_lws,
			header_name,
			space_before_header_value,
			header_value,
			expecting_newline_2,
			expecting_newline_3
		} state_;
	};

}
