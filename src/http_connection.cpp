#include "include/http_connection.hpp"
#include "include/escape_string.hpp"
#include "include/io_service_pool.hpp"
#include "include/logging.hpp"
#include "include/http_server.hpp"

namespace av_router {

	http_connection::http_connection(boost::asio::io_service& io, http_server& serv, http_connection_manager* connection_man)
		: m_io_service(io)
		, m_server(serv)
		, m_socket(io)
		, m_connection_manager(connection_man)
		, m_abort(false)
	{}

	http_connection::~http_connection()
	{
		LOG_DBG << "destruct http connection!";
	}

	void http_connection::start()
	{
		m_request.consume(m_request.size());
		m_abort = false;

		boost::system::error_code ignore_ec;
		m_socket.set_option(tcp::no_delay(true), ignore_ec);
		if (ignore_ec)
			LOG_ERR << "http_connection::start, Set option to nodelay, error message :" << ignore_ec.message();

		boost::asio::async_read_until(m_socket, m_request, "\r\n\r\n",
			boost::bind(&http_connection::handle_read_headers,
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
			)
			);
	}

	void http_connection::stop()
	{
		boost::system::error_code ignore_ec;
		m_abort = true;
		m_socket.close(ignore_ec);
	}

	tcp::socket& http_connection::socket()
	{
		return m_socket;
	}

	void http_connection::handle_read_headers(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		// 出错处理.
		if (error || m_abort)
		{
			m_connection_manager->stop(shared_from_this());
			return;
		}

		// 复制http头缓冲区.
		std::vector<char> buffer;
		buffer.resize(bytes_transferred + 1);
		buffer[bytes_transferred] = 0;
		m_request.sgetn(&buffer[0], bytes_transferred);

		boost::tribool result;
		boost::tie(result, boost::tuples::ignore) = m_request_parser.parse(m_http_request, buffer.begin(), buffer.end());
		if (!result || result == boost::indeterminate)
		{
			// 断开.
			m_connection_manager->stop(shared_from_this());
			return;
		}

		m_http_request.normalise();

		if (m_http_request.method == "post")
		{
			// NOTE: 限制 body 的大小到 64KiB
			auto content_length = m_http_request.content_length;
			if (content_length == 0 || content_length >= 65536)
			{
				// 断开, POST 必须要有 content_length
				// 暴力断开没事, 首先浏览器不会发这种垃圾请求
				// 第二, 如果在 nginx 后面, 暴力断开 nginx 会返回 503 错误
				m_connection_manager->stop(shared_from_this());
				return;
			}

			auto already_got = m_request.size();
			if (already_got >= content_length)
			{
				handle_read_body(error, content_length);
			}
			else
			{
				// 读取 body
				boost::asio::async_read(m_socket, m_request, boost::asio::transfer_exactly(content_length - already_got),
					boost::bind(&http_connection::handle_read_body,
					shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
					)
					);
			}
		}
		else
		{
			if (!m_server.handle_request(m_http_request, shared_from_this()))
			{
				// 断开. 反正暴力就对了, 越暴力越不容易被人攻击
				m_connection_manager->stop(shared_from_this());
				return;
			}

			if (m_http_request.keep_alive)
			{
				// 继续读取下一个请求.
				boost::asio::async_read_until(m_socket, m_request, "\r\n\r\n",
					boost::bind(&http_connection::handle_read_headers,
					shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
					)
					);
			}
		}
	}

	void http_connection::handle_read_body(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		// 出错处理.
		if (error || m_abort)
		{
			m_connection_manager->stop(shared_from_this());
			return;
		}

		m_http_request.body.resize(m_http_request.content_length);
		m_request.sgetn(&m_http_request.body[0], m_http_request.content_length);

		if (!m_server.handle_request(m_http_request, shared_from_this()))
		{
			// 断开. 反正暴力就对了, 越暴力越不容易被人攻击
			m_connection_manager->stop(shared_from_this());
			return;
		}

		if (m_http_request.keep_alive)
		{
			// 继续读取下一个请求.
			boost::asio::async_read_until(m_socket, m_request, "\r\n\r\n",
				boost::bind(&http_connection::handle_read_headers,
				shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
				)
				);
		}
	}

	void http_connection::write_response(const std::string& body)
	{
		std::ostream out(&m_response);

		out << "HTTP/" << m_http_request.http_version_major << "." << m_http_request.http_version_minor << " 200 OK\r\n";
		out << "Content-Type: application/json\r\n";
		out << "Content-Length: " << body.length() << "\r\n";
		out << "\r\n";

		out << body;

		boost::asio::async_write(m_socket, m_response,
			boost::bind(&http_connection::handle_write_http,
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
			)
			);

	}

	void http_connection::write_response(const std::string& head, const std::string& body)
	{
		std::ostream out(&m_response);

		out << head;
		out << body;

		boost::asio::async_write(m_socket, m_response,
			boost::bind(&http_connection::handle_write_http,
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
			)
			);

	}

	void http_connection::handle_write_http(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		// 出错处理.
		if (error || m_abort || !m_http_request.keep_alive)
		{
			m_connection_manager->stop(shared_from_this());
			return;
		}
	}
}
