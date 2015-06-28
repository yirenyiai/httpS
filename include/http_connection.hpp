//
// Copyright (C) 2013 Jack.
//
// Author: jack
// Email:  jack.wgm@gmail.com
//

#pragma once

#include <set>

#include <boost/noncopyable.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/progress.hpp>
#include <boost/tokenizer.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
using boost::asio::ip::tcp;
#include <boost/date_time.hpp>
using namespace boost::posix_time;

#include "internal.hpp"
#include "http_helper.hpp"
#include "logging.hpp"

namespace http {

	class http_server;
	class http_connection_manager;

	class http_connection
		: public boost::enable_shared_from_this<http_connection>
		, public boost::noncopyable
	{
	public:
		explicit http_connection(boost::asio::io_service& io, http_server&, http_connection_manager*);
		~http_connection();

	public:
		void start();
		void stop();

		tcp::socket& socket();

	public:
		// 为了简化实现, 只返回 HTTP 200
		void write_response(const std::string&);
		// 如果需要。请自行设置HTTP协议头
		void write_response(const std::string& head, const std::string& body);
	private:
		void handle_read_headers(const boost::system::error_code& error, std::size_t bytes_transferred);
		void handle_read_body(const boost::system::error_code& error, std::size_t bytes_transferred);
		void handle_write_http(const boost::system::error_code& error, std::size_t bytes_transferred);
	private:
		boost::asio::io_service& m_io_service;
		http_server& m_server;
		tcp::socket m_socket;
		http_connection_manager* m_connection_manager;
		boost::asio::streambuf m_request;
		boost::asio::streambuf m_response;
		request_parser m_request_parser;
		request m_http_request;
		bool m_abort;
	};


	typedef boost::shared_ptr<http_connection> http_connection_ptr;
	class http_connection_manager
		: private boost::noncopyable
	{
	public:
		/// Add the specified connection to the manager and start it.
		void start(http_connection_ptr c)
		{
			boost::mutex::scoped_lock l(m_mutex);
			m_connections.insert(c);
			c->start();
		}

		/// Stop the specified connection.
		void stop(http_connection_ptr c)
		{
			boost::mutex::scoped_lock l(m_mutex);
			if (m_connections.find(c) != m_connections.end())
				m_connections.erase(c);
			c->stop();
		}

		/// Stop all connections.
		void stop_all()
		{
			boost::mutex::scoped_lock l(m_mutex);
			std::for_each(m_connections.begin(), m_connections.end(),
				boost::bind(&http_connection::stop, _1));
			m_connections.clear();
		}

		void tick()
		{
			boost::mutex m_mutex;
		}

	private:
		boost::mutex m_mutex;
		std::set<http_connection_ptr> m_connections;
	};

}
